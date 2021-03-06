/*
 * Copyright 2004-2021 the Pacemaker project contributors
 *
 * The version control history for this file may have further details.
 *
 * This source code is licensed under the GNU General Public License version 2
 * or later (GPLv2+) WITHOUT ANY WARRANTY.
 */

#include <crm_resource.h>
#include <crm/common/lists_internal.h>
#include <crm/common/xml_internal.h>
#include <crm/common/output_internal.h>

#define cons_string(x) x?x:"NA"
void
cli_resource_print_cts_constraints(pe_working_set_t * data_set)
{
    pcmk__output_t *out = data_set->priv;
    xmlNode *xml_obj = NULL;
    xmlNode *lifetime = NULL;
    xmlNode *cib_constraints = get_object_root(XML_CIB_TAG_CONSTRAINTS, data_set->input);

    for (xml_obj = pcmk__xe_first_child(cib_constraints); xml_obj != NULL;
         xml_obj = pcmk__xe_next(xml_obj)) {
        const char *id = crm_element_value(xml_obj, XML_ATTR_ID);

        if (id == NULL) {
            continue;
        }

        // @COMPAT lifetime is deprecated
        lifetime = first_named_child(xml_obj, "lifetime");
        if (pe_evaluate_rules(lifetime, NULL, data_set->now, NULL) == FALSE) {
            continue;
        }

        if (!pcmk__str_eq(XML_CONS_TAG_RSC_DEPEND, crm_element_name(xml_obj), pcmk__str_casei)) {
            continue;
        }

        out->info(out, "Constraint %s %s %s %s %s %s %s",
                  crm_element_name(xml_obj),
                  cons_string(crm_element_value(xml_obj, XML_ATTR_ID)),
                  cons_string(crm_element_value(xml_obj, XML_COLOC_ATTR_SOURCE)),
                  cons_string(crm_element_value(xml_obj, XML_COLOC_ATTR_TARGET)),
                  cons_string(crm_element_value(xml_obj, XML_RULE_ATTR_SCORE)),
                  cons_string(crm_element_value(xml_obj, XML_COLOC_ATTR_SOURCE_ROLE)),
                  cons_string(crm_element_value(xml_obj, XML_COLOC_ATTR_TARGET_ROLE)));
    }
}

void
cli_resource_print_cts(pcmk__output_t *out, pe_resource_t * rsc)
{
    GListPtr lpc = NULL;
    const char *host = NULL;
    bool needs_quorum = TRUE;
    const char *rtype = crm_element_value(rsc->xml, XML_ATTR_TYPE);
    const char *rprov = crm_element_value(rsc->xml, XML_AGENT_ATTR_PROVIDER);
    const char *rclass = crm_element_value(rsc->xml, XML_AGENT_ATTR_CLASS);
    pe_node_t *node = pe__current_node(rsc);

    if (pcmk__str_eq(rclass, PCMK_RESOURCE_CLASS_STONITH, pcmk__str_casei)) {
        needs_quorum = FALSE;
    } else {
        // @TODO check requires in resource meta-data and rsc_defaults
    }

    if (node != NULL) {
        host = node->details->uname;
    }

    out->info(out, "Resource: %s %s %s %s %s %s %s %s %d %lld 0x%.16llx",
              crm_element_name(rsc->xml), rsc->id,
              rsc->clone_name ? rsc->clone_name : rsc->id, rsc->parent ? rsc->parent->id : "NA",
              rprov ? rprov : "NA", rclass, rtype, host ? host : "NA", needs_quorum, rsc->flags,
              rsc->flags);

    for (lpc = rsc->children; lpc != NULL; lpc = lpc->next) {
        pe_resource_t *child = (pe_resource_t *) lpc->data;

        cli_resource_print_cts(out, child);
    }
}

// \return Standard Pacemaker return code
int
cli_resource_print_operations(const char *rsc_id, const char *host_uname,
                              bool active, pe_working_set_t * data_set)
{
    pcmk__output_t *out = data_set->priv;
    int rc = pcmk_rc_no_output;
    GListPtr ops = find_operations(rsc_id, host_uname, active, data_set);

    if (!ops) {
        return rc;
    }

    out->begin_list(out, NULL, NULL, "Resource Operations");
    rc = pcmk_rc_ok;

    for (GListPtr lpc = ops; lpc != NULL; lpc = lpc->next) {
        xmlNode *xml_op = (xmlNode *) lpc->data;
        out->message(out, "node-and-op", data_set, xml_op);
    }

    out->end_list(out);
    return rc;
}

// \return Standard Pacemaker return code
int
cli_resource_print(pe_resource_t *rsc, pe_working_set_t *data_set, bool expanded)
{
    pcmk__output_t *out = data_set->priv;
    unsigned int opts = pe_print_pending;
    GListPtr all = NULL;

    all = g_list_prepend(all, strdup("*"));

    out->begin_list(out, NULL, NULL, "Resource Config");
    out->message(out, crm_map_element_name(rsc->xml), opts, rsc, all, all);
    out->message(out, "resource-config", rsc, !expanded);
    out->end_list(out);

    g_list_free_full(all, free);
    return pcmk_rc_ok;
}

PCMK__OUTPUT_ARGS("attribute-list", "pe_resource_t *", "char *", "GHashTable *")
static int
attribute_list_default(pcmk__output_t *out, va_list args) {
    pe_resource_t *rsc = va_arg(args, pe_resource_t *);
    char *attr = va_arg(args, char *);
    GHashTable *params = va_arg(args, GHashTable *);

    const char *value = NULL;

    if (params != NULL) {
        value = g_hash_table_lookup(params, attr);
    }
    if (value != NULL) {
        out->begin_list(out, NULL, NULL, "Attributes");
        out->list_item(out, attr, "%s", value);
        out->end_list(out);
    } else {
        out->err(out, "Attribute '%s' not found for '%s'", attr, rsc->id);
    }

    return pcmk_rc_ok;
}

PCMK__OUTPUT_ARGS("attribute-list", "pe_resource_t *", "char *", "GHashTable *")
static int
attribute_list_text(pcmk__output_t *out, va_list args) {
    pe_resource_t *rsc = va_arg(args, pe_resource_t *);
    char *attr = va_arg(args, char *);
    GHashTable *params = va_arg(args, GHashTable *);

    const char *value = NULL;

    if (params != NULL) {
        value = g_hash_table_lookup(params, attr);
    }
    if (value != NULL) {
        out->info(out, "%s", value);
    } else {
        out->err(out, "Attribute '%s' not found for '%s'", attr, rsc->id);
    }

    return pcmk_rc_ok;
}

PCMK__OUTPUT_ARGS("property-list", "pe_resource_t *", "char *")
static int
property_list_default(pcmk__output_t *out, va_list args) {
    pe_resource_t *rsc = va_arg(args, pe_resource_t *);
    char *attr = va_arg(args, char *);

    const char *value = crm_element_value(rsc->xml, attr);

    if (value != NULL) {
        out->begin_list(out, NULL, NULL, "Properties");
        out->list_item(out, attr, "%s", value);
        out->end_list(out);
    }

    return pcmk_rc_ok;
}

PCMK__OUTPUT_ARGS("property-list", "pe_resource_t *", "char *")
static int
property_list_text(pcmk__output_t *out, va_list args) {
    pe_resource_t *rsc = va_arg(args, pe_resource_t *);
    char *attr = va_arg(args, char *);

    const char *value = crm_element_value(rsc->xml, attr);

    if (value != NULL) {
        out->info(out, "%s", value);
    }

    return pcmk_rc_ok;
}

PCMK__OUTPUT_ARGS("resource-check-list", "resource_checks_t *")
static int
resource_check_list_default(pcmk__output_t *out, va_list args) {
    resource_checks_t *checks = va_arg(args, resource_checks_t *);

    pe_resource_t *parent = uber_parent(checks->rsc);
    int rc = pcmk_rc_no_output;
    bool printed = false;

    if (checks->flags != 0 || checks->lock_node != NULL) {
        printed = true;
        out->begin_list(out, NULL, NULL, "Resource Checks");
    }

    if (pcmk_is_set(checks->flags, rsc_remain_stopped)) {
        out->list_item(out, "check", "Configuration specifies '%s' should remain stopped",
                       parent->id);
    }

    if (pcmk_is_set(checks->flags, rsc_unpromotable)) {
        out->list_item(out, "check", "Configuration specifies '%s' should not be promoted",
                       parent->id);
    }

    if (pcmk_is_set(checks->flags, rsc_unmanaged)) {
        out->list_item(out, "check", "Configuration prevents cluster from stopping or starting unmanaged '%s'",
                       parent->id);
    }

    if (checks->lock_node) {
        out->list_item(out, "check", "'%s' is locked to node %s due to shutdown",
                       parent->id, checks->lock_node);
    }

    if (printed) {
        out->end_list(out);
        rc = pcmk_rc_ok;
    }

    return rc;
}

PCMK__OUTPUT_ARGS("resource-check-list", "resource_checks_t *")
static int
resource_check_list_xml(pcmk__output_t *out, va_list args) {
    resource_checks_t *checks = va_arg(args, resource_checks_t *);

    pe_resource_t *parent = uber_parent(checks->rsc);
    int rc = pcmk_rc_no_output;

    xmlNodePtr node = pcmk__output_create_xml_node(out, "check",
                                                   "id", parent->id,
                                                   NULL);

    if (pcmk_is_set(checks->flags, rsc_remain_stopped)) {
        crm_xml_add(node, "remain_stopped", "true");
    }

    if (pcmk_is_set(checks->flags, rsc_unpromotable)) {
        crm_xml_add(node, "promotable", "false");
    }

    if (pcmk_is_set(checks->flags, rsc_unmanaged)) {
        crm_xml_add(node, "unmanaged", "true");
    }

    if (checks->lock_node) {
        crm_xml_add(node, "locked-to", checks->lock_node);
    }

    return rc;
}

PCMK__OUTPUT_ARGS("resource-search-list", "GList *", "gchar *")
static int
resource_search_list_default(pcmk__output_t *out, va_list args)
{
    GList *nodes = va_arg(args, GList *);
    gchar *requested_name = va_arg(args, gchar *);

    bool printed = false;
    int rc = pcmk_rc_no_output;

    if (!out->is_quiet(out) && nodes == NULL) {
        out->err(out, "resource %s is NOT running", requested_name);
        return rc;
    }

    for (GList *lpc = nodes; lpc != NULL; lpc = lpc->next) {
        node_info_t *ni = (node_info_t *) lpc->data;

        if (!printed) {
            out->begin_list(out, NULL, NULL, "Nodes");
            printed = true;
            rc = pcmk_rc_ok;
        }

        if (out->is_quiet(out)) {
            out->list_item(out, "node", "%s", ni->node_name);
        } else {
            out->list_item(out, "node", "resource %s is running on: %s%s",
                           requested_name, ni->node_name, ni->promoted ? " Master" : "");
        }
    }

    if (printed) {
        out->end_list(out);
    }

    return rc;
}

PCMK__OUTPUT_ARGS("resource-search-list", "GList *", "gchar *")
static int
resource_search_list_xml(pcmk__output_t *out, va_list args)
{
    GList *nodes = va_arg(args, GList *);
    gchar *requested_name = va_arg(args, gchar *);

    pcmk__output_xml_create_parent(out, "nodes",
                                   "resource", requested_name,
                                   NULL);

    for (GList *lpc = nodes; lpc != NULL; lpc = lpc->next) {
        node_info_t *ni = (node_info_t *) lpc->data;
        xmlNodePtr sub_node = pcmk__output_create_xml_text_node(out, "node", ni->node_name);

        if (ni->promoted) {
            crm_xml_add(sub_node, "state", "promoted");
        }
    }

    return pcmk_rc_ok;
}

PCMK__OUTPUT_ARGS("resource-reasons-list", "cib_t *", "GList *", "pe_resource_t *",
                  "pe_node_t *")
static int
resource_reasons_list_default(pcmk__output_t *out, va_list args)
{
    cib_t *cib_conn = va_arg(args, cib_t *);
    GList *resources = va_arg(args, GList *);
    pe_resource_t *rsc = va_arg(args, pe_resource_t *);
    pe_node_t *node = va_arg(args, pe_node_t *);

    const char *host_uname = (node == NULL)? NULL : node->details->uname;

    out->begin_list(out, NULL, NULL, "Resource Reasons");

    if ((rsc == NULL) && (host_uname == NULL)) {
        GList *lpc = NULL;
        GList *hosts = NULL;

        for (lpc = resources; lpc != NULL; lpc = lpc->next) {
            pe_resource_t *rsc = (pe_resource_t *) lpc->data;
            rsc->fns->location(rsc, &hosts, TRUE);

            if (hosts == NULL) {
                out->list_item(out, "reason", "Resource %s is not running", rsc->id);
            } else {
                out->list_item(out, "reason", "Resource %s is running", rsc->id);
            }

            cli_resource_check(out, cib_conn, rsc);
            g_list_free(hosts);
            hosts = NULL;
        }

    } else if ((rsc != NULL) && (host_uname != NULL)) {
        if (resource_is_running_on(rsc, host_uname)) {
            out->list_item(out, "reason", "Resource %s is running on host %s",
                           rsc->id, host_uname);
        } else {
            out->list_item(out, "reason", "Resource %s is not running on host %s",
                           rsc->id, host_uname);
        }

        cli_resource_check(out, cib_conn, rsc);

    } else if ((rsc == NULL) && (host_uname != NULL)) {
        const char* host_uname =  node->details->uname;
        GList *allResources = node->details->allocated_rsc;
        GList *activeResources = node->details->running_rsc;
        GList *unactiveResources = pcmk__subtract_lists(allResources, activeResources, (GCompareFunc) strcmp);
        GList *lpc = NULL;

        for (lpc = activeResources; lpc != NULL; lpc = lpc->next) {
            pe_resource_t *rsc = (pe_resource_t *) lpc->data;
            out->list_item(out, "reason", "Resource %s is running on host %s",
                           rsc->id, host_uname);
            cli_resource_check(out, cib_conn, rsc);
        }

        for(lpc = unactiveResources; lpc != NULL; lpc = lpc->next) {
            pe_resource_t *rsc = (pe_resource_t *) lpc->data;
            out->list_item(out, "reason", "Resource %s is assigned to host %s but not running",
                           rsc->id, host_uname);
            cli_resource_check(out, cib_conn, rsc);
        }

        g_list_free(allResources);
        g_list_free(activeResources);
        g_list_free(unactiveResources);

    } else if ((rsc != NULL) && (host_uname == NULL)) {
        GList *hosts = NULL;

        rsc->fns->location(rsc, &hosts, TRUE);
        out->list_item(out, "reason", "Resource %s is %srunning",
                       rsc->id, (hosts? "" : "not "));
        cli_resource_check(out, cib_conn, rsc);
        g_list_free(hosts);
    }

    out->end_list(out);
    return pcmk_rc_ok;
}

PCMK__OUTPUT_ARGS("resource-reasons-list", "cib_t *", "GList *", "pe_resource_t *",
                  "pe_node_t *")
static int
resource_reasons_list_xml(pcmk__output_t *out, va_list args)
{
    cib_t *cib_conn = va_arg(args, cib_t *);
    GList *resources = va_arg(args, GList *);
    pe_resource_t *rsc = va_arg(args, pe_resource_t *);
    pe_node_t *node = va_arg(args, pe_node_t *);

    const char *host_uname = (node == NULL)? NULL : node->details->uname;

    xmlNodePtr xml_node = pcmk__output_xml_create_parent(out, "reason", NULL);

    if ((rsc == NULL) && (host_uname == NULL)) {
        GList *lpc = NULL;
        GList *hosts = NULL;

        pcmk__output_xml_create_parent(out, "resources", NULL);

        for (lpc = resources; lpc != NULL; lpc = lpc->next) {
            pe_resource_t *rsc = (pe_resource_t *) lpc->data;

            rsc->fns->location(rsc, &hosts, TRUE);

            pcmk__output_xml_create_parent(out, "resource",
                                           "id", rsc->id,
                                           "running", pcmk__btoa(hosts != NULL),
                                           NULL);

            cli_resource_check(out, cib_conn, rsc);
            pcmk__output_xml_pop_parent(out);
            g_list_free(hosts);
            hosts = NULL;
        }

        pcmk__output_xml_pop_parent(out);

    } else if ((rsc != NULL) && (host_uname != NULL)) {
        if (resource_is_running_on(rsc, host_uname)) {
            crm_xml_add(xml_node, "running_on", host_uname);
        }

        cli_resource_check(out, cib_conn, rsc);

    } else if ((rsc == NULL) && (host_uname != NULL)) {
        const char* host_uname =  node->details->uname;
        GList *allResources = node->details->allocated_rsc;
        GList *activeResources = node->details->running_rsc;
        GList *unactiveResources = pcmk__subtract_lists(allResources, activeResources, (GCompareFunc) strcmp);
        GList *lpc = NULL;

        pcmk__output_xml_create_parent(out, "resources", NULL);

        for (lpc = activeResources; lpc != NULL; lpc = lpc->next) {
            pe_resource_t *rsc = (pe_resource_t *) lpc->data;

            pcmk__output_xml_create_parent(out, "resource",
                                           "id", rsc->id,
                                           "running", "true",
                                           "host", host_uname,
                                           NULL);

            cli_resource_check(out, cib_conn, rsc);
            pcmk__output_xml_pop_parent(out);
        }

        for(lpc = unactiveResources; lpc != NULL; lpc = lpc->next) {
            pe_resource_t *rsc = (pe_resource_t *) lpc->data;

            pcmk__output_xml_create_parent(out, "resource",
                                           "id", rsc->id,
                                           "running", "false",
                                           "host", host_uname,
                                           NULL);

            cli_resource_check(out, cib_conn, rsc);
            pcmk__output_xml_pop_parent(out);
        }

        pcmk__output_xml_pop_parent(out);
        g_list_free(allResources);
        g_list_free(activeResources);
        g_list_free(unactiveResources);

    } else if ((rsc != NULL) && (host_uname == NULL)) {
        GList *hosts = NULL;

        rsc->fns->location(rsc, &hosts, TRUE);
        crm_xml_add(xml_node, "running", pcmk__btoa(hosts != NULL));
        cli_resource_check(out, cib_conn, rsc);
        g_list_free(hosts);
    }

    return pcmk_rc_ok;
}

static void
add_resource_name(pcmk__output_t *out, pe_resource_t *rsc) {
    if (rsc->children == NULL) {
        out->list_item(out, "resource", "%s", rsc->id);
    } else {
        for (GListPtr lpc = rsc->children; lpc != NULL; lpc = lpc->next) {
            pe_resource_t *child = (pe_resource_t *) lpc->data;
            add_resource_name(out, child);
        }
    }
}

PCMK__OUTPUT_ARGS("resource-names-list", "GList *")
static int
resource_names(pcmk__output_t *out, va_list args) {
    GList *resources = va_arg(args, GList *);

    if (resources == NULL) {
        out->err(out, "NO resources configured\n");
        return pcmk_rc_no_output;
    }

    out->begin_list(out, NULL, NULL, "Resource Names");

    for (GList *lpc = resources; lpc != NULL; lpc = lpc->next) {
        pe_resource_t *rsc = (pe_resource_t *) lpc->data;
        add_resource_name(out, rsc);
    }

    out->end_list(out);
    return pcmk_rc_ok;
}

static pcmk__message_entry_t fmt_functions[] = {
    { "attribute-list", "default", attribute_list_default },
    { "attribute-list", "text", attribute_list_text },
    { "property-list", "default", property_list_default },
    { "property-list", "text", property_list_text },
    { "resource-check-list", "default", resource_check_list_default },
    { "resource-check-list", "xml", resource_check_list_xml },
    { "resource-search-list", "default", resource_search_list_default },
    { "resource-search-list", "xml", resource_search_list_xml },
    { "resource-reasons-list", "default", resource_reasons_list_default },
    { "resource-reasons-list", "xml", resource_reasons_list_xml },
    { "resource-names-list", "default", resource_names },

    { NULL, NULL, NULL }
};

void
crm_resource_register_messages(pcmk__output_t *out) {
    pcmk__register_messages(out, fmt_functions);
}
