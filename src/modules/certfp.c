/*
 * Certificate Fingerprint Module
 * This grabs the SHA256 fingerprint of the SSL/TLS client certificate
 * the user is using, shares it with the other servers (and rest of
 * UnrealIRCd) and shows it in /WHOIS etc.
 *
 * (C) Copyright 2014-2015 The UnrealIRCd team (Syzop, DBoyz, Nath and others)
 * 
 * License: GPLv2
 */

#include "unrealircd.h"

ModuleHeader MOD_HEADER(certfp)
  = {
	"certfp",
	"$Id$",
	"Certificate fingerprint",
	"3.2-b8-1",
	NULL 
    };

/* Forward declarations */
void certfp_free(ModData *m);
char *certfp_serialize(ModData *m);
void certfp_unserialize(char *str, ModData *m);
int certfp_connect(aClient *sptr);
int certfp_whois(aClient *sptr, aClient *acptr);

ModDataInfo *certfp_md; /* Module Data structure which we acquire */

#define WHOISCERTFP_STRING ":%s 276 %s %s :has client certificate fingerprint %s"

DLLFUNC int MOD_INIT(certfp)(ModuleInfo *modinfo)
{
ModDataInfo mreq;

	memset(&mreq, 0, sizeof(mreq));
	mreq.name = "certfp";
	mreq.free = certfp_free;
	mreq.serialize = certfp_serialize;
	mreq.unserialize = certfp_unserialize;
	mreq.sync = 1;
	mreq.type = MODDATATYPE_CLIENT;
	certfp_md = ModDataAdd(modinfo->handle, mreq);
	if (!certfp_md)
		abort();

	HookAddEx(modinfo->handle, HOOKTYPE_LOCAL_CONNECT, certfp_connect);
	HookAddEx(modinfo->handle, HOOKTYPE_WHOIS, certfp_whois);

	return MOD_SUCCESS;
}

DLLFUNC int MOD_LOAD(certfp)(int module_load)
{
	return MOD_SUCCESS;
}


DLLFUNC int MOD_UNLOAD(certfp)(int module_unload)
{
	return MOD_SUCCESS;
}

/* 
 * Obtain client's fingerprint.
 */
char *get_fingerprint_for_client(aClient *cptr)
{
	unsigned int n;
	unsigned int l;
	unsigned char md[EVP_MAX_MD_SIZE];
	static char hex[EVP_MAX_MD_SIZE * 2 + 1];
	char hexchars[16] = "0123456789abcdef";
	const EVP_MD *digest = EVP_sha256();
	X509 *x509_clientcert = NULL;

	if (!MyConnect(cptr) || !cptr->ssl)
		return NULL;
	
	x509_clientcert = SSL_get_peer_certificate((SSL *)cptr->ssl);

	if (x509_clientcert)
	{
		if (X509_digest(x509_clientcert, digest, md, &n)) {
			int j = 0;
			for	(l=0; l<n; l++) {
				hex[j++] = hexchars[(md[l] >> 4) & 0xF];
				hex[j++] = hexchars[md[l] & 0xF];
			}
			hex[j] = '\0';
			X509_free(x509_clientcert);
			return hex;
		}
		X509_free(x509_clientcert);
	}
	return NULL;
}

int certfp_connect(aClient *acptr)
{
	if (IsSecure(acptr))
	{
		char *fp = get_fingerprint_for_client(acptr);

		if (!fp)
			return 0; /* wtf? */

		moddata_client_set(acptr, "certfp", fp); /* set & broadcast */
		sendnotice(acptr, "*** Your SSL fingerprint is %s", fp);
	}
	return 0;
}

int certfp_whois(aClient *sptr, aClient *acptr)
{
	char *fp = moddata_client_get(acptr, "certfp");
	
	if (fp)
		sendto_one(sptr, WHOISCERTFP_STRING, me.name, sptr->name, acptr->name, fp);
	return 0;
}

void certfp_free(ModData *m)
{
	if (m->str)
		MyFree(m->str);
	m->str = NULL;
}

char *certfp_serialize(ModData *m)
{
	if (!m->str)
		return NULL;
	return m->str;
}

void certfp_unserialize(char *str, ModData *m)
{
	if (m->str)
		MyFree(m->str);
	m->str = strdup(str);
}
