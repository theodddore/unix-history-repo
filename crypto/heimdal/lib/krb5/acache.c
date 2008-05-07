/*
 * Copyright (c) 2004 - 2007 Kungliga Tekniska H�gskolan
 * (Royal Institute of Technology, Stockholm, Sweden). 
 * All rights reserved. 
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met: 
 *
 * 1. Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright 
 *    notice, this list of conditions and the following disclaimer in the 
 *    documentation and/or other materials provided with the distribution. 
 *
 * 3. Neither the name of the Institute nor the names of its contributors 
 *    may be used to endorse or promote products derived from this software 
 *    without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND 
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE 
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS 
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY 
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE. 
 */

#include "krb5_locl.h"
#include <krb5_ccapi.h>
#ifdef HAVE_DLFCN_H
#include <dlfcn.h>
#endif

RCSID("$Id: acache.c 22099 2007-12-03 17:14:34Z lha $");

/* XXX should we fetch these for each open ? */
static HEIMDAL_MUTEX acc_mutex = HEIMDAL_MUTEX_INITIALIZER;
static cc_initialize_func init_func;

#ifdef HAVE_DLOPEN
static void *cc_handle; 
#endif

typedef struct krb5_acc {
    char *cache_name;
    cc_context_t context;
    cc_ccache_t ccache;
} krb5_acc;

static krb5_error_code acc_close(krb5_context, krb5_ccache);

#define ACACHE(X) ((krb5_acc *)(X)->data.data)

static const struct {
    cc_int32 error;
    krb5_error_code ret;
} cc_errors[] = {
    { ccErrBadName,		KRB5_CC_BADNAME },
    { ccErrCredentialsNotFound,	KRB5_CC_NOTFOUND },
    { ccErrCCacheNotFound,	KRB5_FCC_NOFILE },
    { ccErrContextNotFound,	KRB5_CC_NOTFOUND },
    { ccIteratorEnd,		KRB5_CC_END },
    { ccErrNoMem,		KRB5_CC_NOMEM },
    { ccErrServerUnavailable,	KRB5_CC_NOSUPP },
    { ccNoError,		0 }
};

static krb5_error_code
translate_cc_error(krb5_context context, cc_int32 error)
{
    int i;
    krb5_clear_error_string(context);
    for(i = 0; i < sizeof(cc_errors)/sizeof(cc_errors[0]); i++)
	if (cc_errors[i].error == error)
	    return cc_errors[i].ret;
    return KRB5_FCC_INTERNAL;
}

static krb5_error_code
init_ccapi(krb5_context context)
{
    const char *lib;

    HEIMDAL_MUTEX_lock(&acc_mutex);
    if (init_func) {
	HEIMDAL_MUTEX_unlock(&acc_mutex);
	krb5_clear_error_string(context);
	return 0;
    }

    lib = krb5_config_get_string(context, NULL,
				 "libdefaults", "ccapi_library", 
				 NULL);
    if (lib == NULL) {
#ifdef __APPLE__
	lib = "/System/Library/Frameworks/Kerberos.framework/Kerberos";
#else
	lib = "/usr/lib/libkrb5_cc.so";
#endif
    }

#ifdef HAVE_DLOPEN

#ifndef RTLD_LAZY
#define RTLD_LAZY 0
#endif

    cc_handle = dlopen(lib, RTLD_LAZY);
    if (cc_handle == NULL) {
	HEIMDAL_MUTEX_unlock(&acc_mutex);
	krb5_set_error_string(context, "Failed to load %s", lib);
	return KRB5_CC_NOSUPP;
    }

    init_func = (cc_initialize_func)dlsym(cc_handle, "cc_initialize");
    HEIMDAL_MUTEX_unlock(&acc_mutex);
    if (init_func == NULL) {
	krb5_set_error_string(context, "Failed to find cc_initialize"
			      "in %s: %s", lib, dlerror());
	dlclose(cc_handle);
	return KRB5_CC_NOSUPP;
    }

    return 0;
#else
    HEIMDAL_MUTEX_unlock(&acc_mutex);
    krb5_set_error_string(context, "no support for shared object");
    return KRB5_CC_NOSUPP;
#endif
}    

static krb5_error_code
make_cred_from_ccred(krb5_context context,
		     const cc_credentials_v5_t *incred,
		     krb5_creds *cred)
{
    krb5_error_code ret;
    int i;

    memset(cred, 0, sizeof(*cred));

    ret = krb5_parse_name(context, incred->client, &cred->client);
    if (ret)
	goto fail;

    ret = krb5_parse_name(context, incred->server, &cred->server);
    if (ret)
	goto fail;

    cred->session.keytype = incred->keyblock.type;
    cred->session.keyvalue.length = incred->keyblock.length;
    cred->session.keyvalue.data = malloc(incred->keyblock.length);
    if (cred->session.keyvalue.data == NULL)
	goto nomem;
    memcpy(cred->session.keyvalue.data, incred->keyblock.data,
	   incred->keyblock.length);

    cred->times.authtime = incred->authtime;
    cred->times.starttime = incred->starttime;
    cred->times.endtime = incred->endtime;
    cred->times.renew_till = incred->renew_till;

    ret = krb5_data_copy(&cred->ticket,
			 incred->ticket.data,
			 incred->ticket.length);
    if (ret)
	goto nomem;

    ret = krb5_data_copy(&cred->second_ticket,
			 incred->second_ticket.data,
			 incred->second_ticket.length);
    if (ret)
	goto nomem;

    cred->authdata.val = NULL;
    cred->authdata.len = 0;
    
    cred->addresses.val = NULL;
    cred->addresses.len = 0;
    
    for (i = 0; incred->authdata && incred->authdata[i]; i++)
	;
    
    if (i) {
	cred->authdata.val = calloc(i, sizeof(cred->authdata.val[0]));
	if (cred->authdata.val == NULL)
	    goto nomem;
	cred->authdata.len = i;
	for (i = 0; i < cred->authdata.len; i++) {
	    cred->authdata.val[i].ad_type = incred->authdata[i]->type;
	    ret = krb5_data_copy(&cred->authdata.val[i].ad_data,
				 incred->authdata[i]->data,
				 incred->authdata[i]->length);
	    if (ret)
		goto nomem;
	}
    }
    
    for (i = 0; incred->addresses && incred->addresses[i]; i++)
	;
    
    if (i) {
	cred->addresses.val = calloc(i, sizeof(cred->addresses.val[0]));
	if (cred->addresses.val == NULL)
	    goto nomem;
	cred->addresses.len = i;
	
	for (i = 0; i < cred->addresses.len; i++) {
	    cred->addresses.val[i].addr_type = incred->addresses[i]->type;
	    ret = krb5_data_copy(&cred->addresses.val[i].address,
				 incred->addresses[i]->data,
				 incred->addresses[i]->length);
	    if (ret)
		goto nomem;
	}
    }
    
    cred->flags.i = 0;
    if (incred->ticket_flags & KRB5_CCAPI_TKT_FLG_FORWARDABLE)
	cred->flags.b.forwardable = 1;
    if (incred->ticket_flags & KRB5_CCAPI_TKT_FLG_FORWARDED)
	cred->flags.b.forwarded = 1;
    if (incred->ticket_flags & KRB5_CCAPI_TKT_FLG_PROXIABLE)
	cred->flags.b.proxiable = 1;
    if (incred->ticket_flags & KRB5_CCAPI_TKT_FLG_PROXY)
	cred->flags.b.proxy = 1;
    if (incred->ticket_flags & KRB5_CCAPI_TKT_FLG_MAY_POSTDATE)
	cred->flags.b.may_postdate = 1;
    if (incred->ticket_flags & KRB5_CCAPI_TKT_FLG_POSTDATED)
	cred->flags.b.postdated = 1;
    if (incred->ticket_flags & KRB5_CCAPI_TKT_FLG_INVALID)
	cred->flags.b.invalid = 1;
    if (incred->ticket_flags & KRB5_CCAPI_TKT_FLG_RENEWABLE)
	cred->flags.b.renewable = 1;
    if (incred->ticket_flags & KRB5_CCAPI_TKT_FLG_INITIAL)
	cred->flags.b.initial = 1;
    if (incred->ticket_flags & KRB5_CCAPI_TKT_FLG_PRE_AUTH)
	cred->flags.b.pre_authent = 1;
    if (incred->ticket_flags & KRB5_CCAPI_TKT_FLG_HW_AUTH)
	cred->flags.b.hw_authent = 1;
    if (incred->ticket_flags & KRB5_CCAPI_TKT_FLG_TRANSIT_POLICY_CHECKED)
	cred->flags.b.transited_policy_checked = 1;
    if (incred->ticket_flags & KRB5_CCAPI_TKT_FLG_OK_AS_DELEGATE)
	cred->flags.b.ok_as_delegate = 1;
    if (incred->ticket_flags & KRB5_CCAPI_TKT_FLG_ANONYMOUS)
	cred->flags.b.anonymous = 1;

    return 0;
    
nomem:
    ret = ENOMEM;
    krb5_set_error_string(context, "malloc - out of memory");
    
fail:
    krb5_free_cred_contents(context, cred);
    return ret;
}

static void
free_ccred(cc_credentials_v5_t *cred)
{
    int i;

    if (cred->addresses) {
	for (i = 0; cred->addresses[i] != 0; i++) {
	    if (cred->addresses[i]->data)
		free(cred->addresses[i]->data);
	    free(cred->addresses[i]);
	}
	free(cred->addresses);
    }
    if (cred->server)
	free(cred->server);
    if (cred->client)
	free(cred->client);
    memset(cred, 0, sizeof(*cred));
}

static krb5_error_code
make_ccred_from_cred(krb5_context context,
		     const krb5_creds *incred,
		     cc_credentials_v5_t *cred)
{
    krb5_error_code ret;
    int i;

    memset(cred, 0, sizeof(*cred));

    ret = krb5_unparse_name(context, incred->client, &cred->client);
    if (ret)
	goto fail;

    ret = krb5_unparse_name(context, incred->server, &cred->server);
    if (ret)
	goto fail;

    cred->keyblock.type = incred->session.keytype;
    cred->keyblock.length = incred->session.keyvalue.length;
    cred->keyblock.data = incred->session.keyvalue.data;

    cred->authtime = incred->times.authtime;
    cred->starttime = incred->times.starttime;
    cred->endtime = incred->times.endtime;
    cred->renew_till = incred->times.renew_till;

    cred->ticket.length = incred->ticket.length;
    cred->ticket.data = incred->ticket.data;

    cred->second_ticket.length = incred->second_ticket.length;
    cred->second_ticket.data = incred->second_ticket.data;

    /* XXX this one should also be filled in */
    cred->authdata = NULL;
    
    cred->addresses = calloc(incred->addresses.len + 1, 
			     sizeof(cred->addresses[0]));
    if (cred->addresses == NULL) {

	ret = ENOMEM;
	goto fail;
    }

    for (i = 0; i < incred->addresses.len; i++) {
	cc_data *addr;
	addr = malloc(sizeof(*addr));
	if (addr == NULL) {
	    ret = ENOMEM;
	    goto fail;
	}
	addr->type = incred->addresses.val[i].addr_type;
	addr->length = incred->addresses.val[i].address.length;
	addr->data = malloc(addr->length);
	if (addr->data == NULL) {
	    ret = ENOMEM;
	    goto fail;
	}
	memcpy(addr->data, incred->addresses.val[i].address.data, 
	       addr->length);
	cred->addresses[i] = addr;
    }
    cred->addresses[i] = NULL;

    cred->ticket_flags = 0;
    if (incred->flags.b.forwardable)
	cred->ticket_flags |= KRB5_CCAPI_TKT_FLG_FORWARDABLE;
    if (incred->flags.b.forwarded)
	cred->ticket_flags |= KRB5_CCAPI_TKT_FLG_FORWARDED;
    if (incred->flags.b.proxiable)
	cred->ticket_flags |= KRB5_CCAPI_TKT_FLG_PROXIABLE;
    if (incred->flags.b.proxy)
	cred->ticket_flags |= KRB5_CCAPI_TKT_FLG_PROXY;
    if (incred->flags.b.may_postdate)
	cred->ticket_flags |= KRB5_CCAPI_TKT_FLG_MAY_POSTDATE;
    if (incred->flags.b.postdated)
	cred->ticket_flags |= KRB5_CCAPI_TKT_FLG_POSTDATED;
    if (incred->flags.b.invalid)
	cred->ticket_flags |= KRB5_CCAPI_TKT_FLG_INVALID;
    if (incred->flags.b.renewable)
	cred->ticket_flags |= KRB5_CCAPI_TKT_FLG_RENEWABLE;
    if (incred->flags.b.initial)
	cred->ticket_flags |= KRB5_CCAPI_TKT_FLG_INITIAL;
    if (incred->flags.b.pre_authent)
	cred->ticket_flags |= KRB5_CCAPI_TKT_FLG_PRE_AUTH;
    if (incred->flags.b.hw_authent)
	cred->ticket_flags |= KRB5_CCAPI_TKT_FLG_HW_AUTH;
    if (incred->flags.b.transited_policy_checked)
	cred->ticket_flags |= KRB5_CCAPI_TKT_FLG_TRANSIT_POLICY_CHECKED;
    if (incred->flags.b.ok_as_delegate)
	cred->ticket_flags |= KRB5_CCAPI_TKT_FLG_OK_AS_DELEGATE;
    if (incred->flags.b.anonymous)
	cred->ticket_flags |= KRB5_CCAPI_TKT_FLG_ANONYMOUS;

    return 0;

fail:    
    free_ccred(cred);

    krb5_clear_error_string(context);
    return ret;
}

static char *
get_cc_name(cc_ccache_t cache)
{
    cc_string_t name;
    cc_int32 error;
    char *str;

    error = (*cache->func->get_name)(cache, &name);
    if (error)
	return NULL;

    str = strdup(name->data);
    (*name->func->release)(name);
    return str;
}


static const char*
acc_get_name(krb5_context context,
	     krb5_ccache id)
{
    krb5_acc *a = ACACHE(id);
    static char n[255];
    char *name;

    name = get_cc_name(a->ccache);
    if (name == NULL) {
	krb5_set_error_string(context, "malloc: out of memory");
	return NULL;
    }
    strlcpy(n, name, sizeof(n));
    free(name);
    return n;
}

static krb5_error_code
acc_alloc(krb5_context context, krb5_ccache *id)
{
    krb5_error_code ret;
    cc_int32 error;
    krb5_acc *a;

    ret = init_ccapi(context);
    if (ret)
	return ret;

    ret = krb5_data_alloc(&(*id)->data, sizeof(*a));
    if (ret) {
	krb5_clear_error_string(context);
	return ret;
    }
    
    a = ACACHE(*id);

    error = (*init_func)(&a->context, ccapi_version_3, NULL, NULL);
    if (error) {
	krb5_data_free(&(*id)->data);
	return translate_cc_error(context, error);
    }

    a->cache_name = NULL;

    return 0;
}

static krb5_error_code
acc_resolve(krb5_context context, krb5_ccache *id, const char *res)
{
    krb5_error_code ret;
    cc_int32 error;
    krb5_acc *a;

    ret = acc_alloc(context, id);
    if (ret)
	return ret;

    a = ACACHE(*id);

    error = (*a->context->func->open_ccache)(a->context, res,
					     &a->ccache);
    if (error == 0) {
	a->cache_name = get_cc_name(a->ccache);
	if (a->cache_name == NULL) {
	    acc_close(context, *id);
	    *id = NULL;
	    krb5_set_error_string(context, "malloc: out of memory");
	    return ENOMEM;
	}
    } else if (error == ccErrCCacheNotFound) {
	a->ccache = NULL;
	a->cache_name = NULL;
	error = 0;
    } else {
	*id = NULL;
	return translate_cc_error(context, error);
    }

    return 0;
}

static krb5_error_code
acc_gen_new(krb5_context context, krb5_ccache *id)
{
    krb5_error_code ret;
    krb5_acc *a;

    ret = acc_alloc(context, id);
    if (ret)
	return ret;

    a = ACACHE(*id);

    a->ccache = NULL;
    a->cache_name = NULL;

    return 0;
}

static krb5_error_code
acc_initialize(krb5_context context,
	       krb5_ccache id,
	       krb5_principal primary_principal)
{
    krb5_acc *a = ACACHE(id);
    krb5_error_code ret;
    int32_t error;
    char *name;

    ret = krb5_unparse_name(context, primary_principal, &name);
    if (ret)
	return ret;

    error = (*a->context->func->create_new_ccache)(a->context,
						   cc_credentials_v5,
						   name,
						   &a->ccache);
    free(name);

    return translate_cc_error(context, error);
}

static krb5_error_code
acc_close(krb5_context context,
	  krb5_ccache id)
{
    krb5_acc *a = ACACHE(id);

    if (a->ccache) {
	(*a->ccache->func->release)(a->ccache);
	a->ccache = NULL;
    }
    if (a->cache_name) {
	free(a->cache_name);
	a->cache_name = NULL;
    }
    (*a->context->func->release)(a->context);
    a->context = NULL;
    krb5_data_free(&id->data);
    return 0;
}

static krb5_error_code
acc_destroy(krb5_context context,
	    krb5_ccache id)
{
    krb5_acc *a = ACACHE(id);
    cc_int32 error = 0;

    if (a->ccache) {
	error = (*a->ccache->func->destroy)(a->ccache);
	a->ccache = NULL;
    }
    if (a->context) {
	error = (a->context->func->release)(a->context);
	a->context = NULL;
    }
    return translate_cc_error(context, error);
}

static krb5_error_code
acc_store_cred(krb5_context context,
	       krb5_ccache id,
	       krb5_creds *creds)
{
    krb5_acc *a = ACACHE(id);
    cc_credentials_union cred;
    cc_credentials_v5_t v5cred;
    krb5_error_code ret;
    cc_int32 error;
    
    if (a->ccache == NULL) {
	krb5_set_error_string(context, "No API credential found");
	return KRB5_CC_NOTFOUND;
    }

    cred.version = cc_credentials_v5;
    cred.credentials.credentials_v5 = &v5cred;

    ret = make_ccred_from_cred(context, 
			       creds,
			       &v5cred);
    if (ret)
	return ret;

    error = (*a->ccache->func->store_credentials)(a->ccache, &cred);
    if (error)
	ret = translate_cc_error(context, error);

    free_ccred(&v5cred);

    return ret;
}

static krb5_error_code
acc_get_principal(krb5_context context,
		  krb5_ccache id,
		  krb5_principal *principal)
{
    krb5_acc *a = ACACHE(id);
    krb5_error_code ret;
    int32_t error;
    cc_string_t name;

    if (a->ccache == NULL) {
	krb5_set_error_string(context, "No API credential found");
	return KRB5_CC_NOTFOUND;
    }

    error = (*a->ccache->func->get_principal)(a->ccache,
					      cc_credentials_v5,
					      &name);
    if (error)
	return translate_cc_error(context, error);
    
    ret = krb5_parse_name(context, name->data, principal);
    
    (*name->func->release)(name);
    return ret;
}

static krb5_error_code
acc_get_first (krb5_context context,
	       krb5_ccache id,
	       krb5_cc_cursor *cursor)
{
    cc_credentials_iterator_t iter;
    krb5_acc *a = ACACHE(id);
    int32_t error;
    
    if (a->ccache == NULL) {
	krb5_set_error_string(context, "No API credential found");
	return KRB5_CC_NOTFOUND;
    }

    error = (*a->ccache->func->new_credentials_iterator)(a->ccache, &iter);
    if (error) {
	krb5_clear_error_string(context);
	return ENOENT;
    }
    *cursor = iter;
    return 0;
}


static krb5_error_code
acc_get_next (krb5_context context,
	      krb5_ccache id,
	      krb5_cc_cursor *cursor,
	      krb5_creds *creds)
{
    cc_credentials_iterator_t iter = *cursor;
    cc_credentials_t cred;
    krb5_error_code ret;
    int32_t error;

    while (1) {
	error = (*iter->func->next)(iter, &cred);
	if (error)
	    return translate_cc_error(context, error);
	if (cred->data->version == cc_credentials_v5)
	    break;
	(*cred->func->release)(cred);
    }

    ret = make_cred_from_ccred(context, 
			       cred->data->credentials.credentials_v5,
			       creds);
    (*cred->func->release)(cred);
    return ret;
}

static krb5_error_code
acc_end_get (krb5_context context,
	     krb5_ccache id,
	     krb5_cc_cursor *cursor)
{
    cc_credentials_iterator_t iter = *cursor;
    (*iter->func->release)(iter);
    return 0;
}

static krb5_error_code
acc_remove_cred(krb5_context context,
		krb5_ccache id,
		krb5_flags which,
		krb5_creds *cred)
{
    cc_credentials_iterator_t iter;
    krb5_acc *a = ACACHE(id);
    cc_credentials_t ccred;
    krb5_error_code ret;
    cc_int32 error;
    char *client, *server;
    
    if (a->ccache == NULL) {
	krb5_set_error_string(context, "No API credential found");
	return KRB5_CC_NOTFOUND;
    }

    if (cred->client) {
	ret = krb5_unparse_name(context, cred->client, &client);
	if (ret)
	    return ret;
    } else
	client = NULL;

    ret = krb5_unparse_name(context, cred->server, &server);
    if (ret) {
	free(client);
	return ret;
    }

    error = (*a->ccache->func->new_credentials_iterator)(a->ccache, &iter);
    if (error) {
	free(server);
	free(client);
	return translate_cc_error(context, error);
    }

    ret = KRB5_CC_NOTFOUND;
    while (1) {
	cc_credentials_v5_t *v5cred;

	error = (*iter->func->next)(iter, &ccred);
	if (error)
	    break;

	if (ccred->data->version != cc_credentials_v5)
	    goto next;

	v5cred = ccred->data->credentials.credentials_v5;

	if (client && strcmp(v5cred->client, client) != 0)
	    goto next;

	if (strcmp(v5cred->server, server) != 0)
	    goto next;

	(*a->ccache->func->remove_credentials)(a->ccache, ccred);
	ret = 0;
    next:
	(*ccred->func->release)(ccred);
    }

    (*iter->func->release)(iter);

    if (ret)
	krb5_set_error_string(context, "Can't find credential %s in cache", 
			      server);
    free(server);
    free(client);

    return ret;
}

static krb5_error_code
acc_set_flags(krb5_context context,
	      krb5_ccache id,
	      krb5_flags flags)
{
    return 0;
}

static krb5_error_code
acc_get_version(krb5_context context,
		krb5_ccache id)
{
    return 0;
}
		    
struct cache_iter {
    cc_context_t context;
    cc_ccache_iterator_t iter;
};

static krb5_error_code
acc_get_cache_first(krb5_context context, krb5_cc_cursor *cursor)
{
    struct cache_iter *iter;
    krb5_error_code ret;
    cc_int32 error;

    ret = init_ccapi(context);
    if (ret)
	return ret;

    iter = calloc(1, sizeof(*iter));
    if (iter == NULL) {
	krb5_set_error_string(context, "malloc - out of memory");
	return ENOMEM;
    }

    error = (*init_func)(&iter->context, ccapi_version_3, NULL, NULL);
    if (error) {
	free(iter);
	return translate_cc_error(context, error);
    }

    error = (*iter->context->func->new_ccache_iterator)(iter->context,
							&iter->iter);
    if (error) {
	free(iter);
	krb5_clear_error_string(context);
	return ENOENT;
    }
    *cursor = iter;
    return 0;
}

static krb5_error_code
acc_get_cache_next(krb5_context context, krb5_cc_cursor cursor, krb5_ccache *id)
{
    struct cache_iter *iter = cursor;
    cc_ccache_t cache;
    krb5_acc *a;
    krb5_error_code ret;
    int32_t error;

    error = (*iter->iter->func->next)(iter->iter, &cache);
    if (error)
	return translate_cc_error(context, error);

    ret = _krb5_cc_allocate(context, &krb5_acc_ops, id);
    if (ret) {
	(*cache->func->release)(cache);
	return ret;
    }

    ret = acc_alloc(context, id);
    if (ret) {
	(*cache->func->release)(cache);
	free(*id);
	return ret;
    }

    a = ACACHE(*id);
    a->ccache = cache;

    a->cache_name = get_cc_name(a->ccache);
    if (a->cache_name == NULL) {
	acc_close(context, *id);
	*id = NULL;
	krb5_set_error_string(context, "malloc: out of memory");
	return ENOMEM;
    }	
    return 0;
}

static krb5_error_code
acc_end_cache_get(krb5_context context, krb5_cc_cursor cursor)
{
    struct cache_iter *iter = cursor;

    (*iter->iter->func->release)(iter->iter);
    iter->iter = NULL;
    (*iter->context->func->release)(iter->context);
    iter->context = NULL;
    free(iter);
    return 0;
}

static krb5_error_code
acc_move(krb5_context context, krb5_ccache from, krb5_ccache to)
{
    krb5_acc *afrom = ACACHE(from);
    krb5_acc *ato = ACACHE(to);
    int32_t error;

    if (ato->ccache == NULL) {
	cc_string_t name;

	error = (*afrom->ccache->func->get_principal)(afrom->ccache,
						      cc_credentials_v5,
						      &name);
	if (error)
	    return translate_cc_error(context, error);
    
	error = (*ato->context->func->create_new_ccache)(ato->context,
							 cc_credentials_v5,
							 name->data,
							 &ato->ccache);
	(*name->func->release)(name);
	if (error)
	    return translate_cc_error(context, error);
    }


    error = (*ato->ccache->func->move)(afrom->ccache, ato->ccache);
    return translate_cc_error(context, error);
}

static krb5_error_code
acc_default_name(krb5_context context, char **str)
{
    krb5_error_code ret;
    cc_context_t cc;
    cc_string_t name;
    int32_t error;

    ret = init_ccapi(context);
    if (ret)
	return ret;

    error = (*init_func)(&cc, ccapi_version_3, NULL, NULL);
    if (error)
	return translate_cc_error(context, error);

    error = (*cc->func->get_default_ccache_name)(cc, &name);
    if (error) {
	(*cc->func->release)(cc);
	return translate_cc_error(context, error);
    }
	
    asprintf(str, "API:%s", name->data);
    (*name->func->release)(name);
    (*cc->func->release)(cc);

    if (*str == NULL) {
	krb5_set_error_string(context, "out of memory");
	return ENOMEM;
    }
    return 0;
}


/**
 * Variable containing the API based credential cache implemention.
 *
 * @ingroup krb5_ccache
 */

const krb5_cc_ops krb5_acc_ops = {
    "API",
    acc_get_name,
    acc_resolve,
    acc_gen_new,
    acc_initialize,
    acc_destroy,
    acc_close,
    acc_store_cred,
    NULL, /* acc_retrieve */
    acc_get_principal,
    acc_get_first,
    acc_get_next,
    acc_end_get,
    acc_remove_cred,
    acc_set_flags,
    acc_get_version,
    acc_get_cache_first,
    acc_get_cache_next,
    acc_end_cache_get,
    acc_move,
    acc_default_name
};
