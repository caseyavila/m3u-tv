#include <libxml/parser.h>
#include <libxml/tree.h>
#include "guide-data.h"

static void print_element_names(xmlNode * a_node)
{
   xmlNode *cur_node = NULL;

   for (cur_node = a_node; cur_node; cur_node =
      cur_node->next) {
      if (cur_node->type == XML_ELEMENT_NODE) {
         printf("node type: Element, name: %s\n",
            cur_node->name);
      }
      print_element_names(cur_node->children);
   }
}

void get_guide_data() {
    xmlDocPtr doc = xmlParseFile("/home/casey/.local/share/m3u-tv/xmltv.xml");

    xmlNodePtr node = xmlDocGetRootElement(doc);

    printf("%s\n", node->name);
    print_element_names(node);

    xmlFreeDoc(doc);
}

