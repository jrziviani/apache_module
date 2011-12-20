#include "httpd.h"
#include "http_config.h"
#include "http_protocol.h"
#include "ap_config.h"

#include "apr_dbd.h"
#include "mod_dbd.h"
#include "apr_strings.h"

#define MAX_HANDLER 3

typedef int (*method_handler)(request_rec *r);

// HTTP Get method handler
static int get_handler(request_rec *r);

// HTTP Post method handler
static int post_handler(request_rec *r);

// HTTP Put method handler
static int put_handler(request_rec *r);

/* optional function - look it up once in post_config */
static ap_dbd_t *(*authn_dbd_acquire_fn)(request_rec*) = NULL;
static void (*authn_dbd_prepare_fn)(server_rec*, const char*, const char*) = NULL;

static int poc_rest_handler(request_rec *r)
{
    if (strcmp(r->handler, "poc_rest")) {
        return DECLINED;
    }

    // as per httpd.h r->method_number gives a numeric representation of http
    // method: 0 - get, 1 - put, 2 - post, 3 - delete, etc
    method_handler methods[MAX_HANDLER] = { &get_handler, &put_handler, &post_handler };

    // accpet only PUT, POST and GET
    if (r->method_number >= MAX_HANDLER || r->method_number < 0) {
        return DECLINED;
    }

    // call the handler function
    return methods[r->method_number](r);
}

static int get_handler(request_rec *r)
{
    ap_dbd_t* dbd = NULL;           // database handler
    apr_status_t rv;
    apr_dbd_results_t* res = NULL;
    apr_dbd_prepared_t* statement;
    apr_dbd_row_t *row = NULL;
    int i = 0;
    int n = 0;
    char* query = r->args;          // query string

    // this is the response type we will send back
    r->content_type = "application/json";

    // return OK if only header requested or no argument
    if (r->header_only) return OK;
    if (r->args == 0) return OK;

    authn_dbd_acquire_fn = APR_RETRIEVE_OPTIONAL_FN(ap_dbd_acquire);
    authn_dbd_prepare_fn = APR_RETRIEVE_OPTIONAL_FN(ap_dbd_prepare);

    if ((dbd = authn_dbd_acquire_fn(r)) == NULL)
        return HTTP_INTERNAL_SERVER_ERROR;

    // user wants to see all items
    if (strncmp(query, "list=all", strlen("list=all")) == 0) {

        const char* id;
        const char* name;

        // query all items from the database
        apr_dbd_select(dbd->driver, r->pool, dbd->handle, &res, "SELECT * FROM teams", 0);

        // format the result in JSON format
        ap_rputs("{\"teams\":[\n", r);
        for (rv = apr_dbd_get_row(dbd->driver, r->pool, res, &row, -1);
             rv != -1;
             rv = apr_dbd_get_row(dbd->driver, r->pool, res, &row, -1)) {

            id = apr_dbd_get_entry(dbd->driver, row, 0);
            name = apr_dbd_get_entry(dbd->driver, row, 1);

            ap_rprintf(r, "{\"id\":\"%s\",\"team\":\"%s\"},",
                    (id == NULL) ? "null" : id,
                    (name == NULL) ? "null" : name);
        }
        ap_rputs("0\n]\n}\n", r);
    
    // user wants a specific item from the db
    } else if (strncmp(query, "list=", strlen("list=")) == 0) {

        int id = 0;
        char id_s[12];
        const char* key;
        const char* value;

        // do some basic sanity check
        if (strlen(query) > 7)
            return HTTP_BAD_REQUEST;

        // extract the id from the query string
        sscanf(query, "list=%d", &id);

        // cannot be a negative id
        if (id <= 0 || id > 12)
             return HTTP_BAD_REQUEST;

        // prepare the query statement
        rv = apr_dbd_prepare(dbd->driver, r->pool, dbd->handle, 
                "SELECT * FROM teams WHERE id = %s", "byid", &statement);
        if (rv) {
            printf("Prepare statement failed!\n%s\n", apr_dbd_error(dbd->driver, dbd->handle, rv));
            return HTTP_INTERNAL_SERVER_ERROR;
        }

        // query the database
        rv = apr_dbd_pvselect(dbd->driver, r->pool, dbd->handle, &res, statement, 0, id_s, NULL);
        if (rv) {
            printf("Exec of prepared statement failed!\n%s\n", apr_dbd_error(dbd->driver, dbd->handle, rv));
            return rv;
        }

        // format the results in JSon format
        int cols = 0;
        for (rv = apr_dbd_get_row(dbd->driver, r->pool, res, &row, -1);
             rv != -1;
             rv = apr_dbd_get_row(dbd->driver, r->pool, res, &row, -1)) {

            cols = apr_dbd_num_cols(dbd->driver, res);
            ap_rputs("{\"team\":\n{", r);
            
            for (n = 0; n < cols; ++n) {

                key = apr_dbd_get_name (dbd->driver, res, n);
                value = apr_dbd_get_entry(dbd->driver, row, n);

                if (key == NULL || value == NULL)
                    continue;
			
                ap_rprintf(r, "\"%s\":\"%s\"", key, value);
                if (n < cols - 1)
                    ap_rputs(", ", r);
            }
            ap_rputs("}\n}", r);
        }
    }

    return OK;
}

// Post http handler
static int post_handler(request_rec *r)
{
    return OK;
}

// Put http handler
static int put_handler(request_rec *r)
{

    return OK;
}

static void poc_rest_register_hooks(apr_pool_t *p)
{
    ap_hook_handler(poc_rest_handler, NULL, NULL, APR_HOOK_MIDDLE);
}

/* Dispatch list for API hooks */
module AP_MODULE_DECLARE_DATA poc_rest_module = {
    STANDARD20_MODULE_STUFF, 
    NULL,                  /* create per-dir    config structures */
    NULL,                  /* merge  per-dir    config structures */
    NULL,                  /* create per-server config structures */
    NULL,                  /* merge  per-server config structures */
    NULL,                  /* table of config file commands       */
    poc_rest_register_hooks  /* register hooks                      */
};

