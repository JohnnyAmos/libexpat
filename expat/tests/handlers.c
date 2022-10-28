/* XML handler functions for the Expat test suite
                            __  __            _
                         ___\ \/ /_ __   __ _| |_
                        / _ \\  /| '_ \ / _` | __|
                       |  __//  \| |_) | (_| | |_
                        \___/_/\_\ .__/ \__,_|\__|
                                 |_| XML parser

   Copyright (c) 2001-2006 Fred L. Drake, Jr. <fdrake@users.sourceforge.net>
   Copyright (c) 2003      Greg Stein <gstein@users.sourceforge.net>
   Copyright (c) 2005-2007 Steven Solie <steven@solie.ca>
   Copyright (c) 2005-2012 Karl Waclawek <karl@waclawek.net>
   Copyright (c) 2016-2022 Sebastian Pipping <sebastian@pipping.org>
   Copyright (c) 2017-2022 Rhodri James <rhodri@wildebeest.org.uk>
   Copyright (c) 2017      Joe Orton <jorton@redhat.com>
   Copyright (c) 2017      José Gutiérrez de la Concha <jose@zeroc.com>
   Copyright (c) 2018      Marco Maggi <marco.maggi-ipsu@poste.it>
   Copyright (c) 2019      David Loffredo <loffredo@steptools.com>
   Copyright (c) 2020      Tim Gates <tim.gates@iress.com>
   Copyright (c) 2021      Dong-hee Na <donghee.na@python.org>
   Licensed under the MIT license:

   Permission is  hereby granted,  free of charge,  to any  person obtaining
   a  copy  of  this  software   and  associated  documentation  files  (the
   "Software"),  to  deal in  the  Software  without restriction,  including
   without  limitation the  rights  to use,  copy,  modify, merge,  publish,
   distribute, sublicense, and/or sell copies of the Software, and to permit
   persons  to whom  the Software  is  furnished to  do so,  subject to  the
   following conditions:

   The above copyright  notice and this permission notice  shall be included
   in all copies or substantial portions of the Software.

   THE  SOFTWARE  IS  PROVIDED  "AS  IS",  WITHOUT  WARRANTY  OF  ANY  KIND,
   EXPRESS  OR IMPLIED,  INCLUDING  BUT  NOT LIMITED  TO  THE WARRANTIES  OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
   NO EVENT SHALL THE AUTHORS OR  COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
   DAMAGES OR  OTHER LIABILITY, WHETHER  IN AN  ACTION OF CONTRACT,  TORT OR
   OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
   USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <string.h>

#include "expat.h"
#include "internal.h"
#include "chardata.h"
#include "structdata.h"
#include "common.h"
#include "handlers.h"

/* Start/End Element Handlers */

void XMLCALL
start_element_event_handler(void *userData, const XML_Char *name,
                            const XML_Char **atts) {
  UNUSED_P(atts);
  CharData_AppendXMLChars((CharData *)userData, name, -1);
}

void XMLCALL
end_element_event_handler(void *userData, const XML_Char *name) {
  CharData *storage = (CharData *)userData;
  CharData_AppendXMLChars(storage, XCS("/"), 1);
  CharData_AppendXMLChars(storage, name, -1);
}

void XMLCALL
start_element_event_handler2(void *userData, const XML_Char *name,
                             const XML_Char **attr) {
  StructData *storage = (StructData *)userData;
  UNUSED_P(attr);
  StructData_AddItem(storage, name, XML_GetCurrentColumnNumber(g_parser),
                     XML_GetCurrentLineNumber(g_parser), STRUCT_START_TAG);
}

void XMLCALL
end_element_event_handler2(void *userData, const XML_Char *name) {
  StructData *storage = (StructData *)userData;
  StructData_AddItem(storage, name, XML_GetCurrentColumnNumber(g_parser),
                     XML_GetCurrentLineNumber(g_parser), STRUCT_END_TAG);
}

/* Text encoding handlers */

int XMLCALL
UnknownEncodingHandler(void *data, const XML_Char *encoding,
                       XML_Encoding *info) {
  UNUSED_P(data);
  if (xcstrcmp(encoding, XCS("unsupported-encoding")) == 0) {
    int i;
    for (i = 0; i < 256; ++i)
      info->map[i] = i;
    info->data = NULL;
    info->convert = NULL;
    info->release = NULL;
    return XML_STATUS_OK;
  }
  return XML_STATUS_ERROR;
}

static void
dummy_release(void *data) {
  UNUSED_P(data);
}

int XMLCALL
UnrecognisedEncodingHandler(void *data, const XML_Char *encoding,
                            XML_Encoding *info) {
  UNUSED_P(data);
  UNUSED_P(encoding);
  info->data = NULL;
  info->convert = NULL;
  info->release = dummy_release;
  return XML_STATUS_ERROR;
}

int XMLCALL
unknown_released_encoding_handler(void *data, const XML_Char *encoding,
                                  XML_Encoding *info) {
  UNUSED_P(data);
  if (! xcstrcmp(encoding, XCS("unsupported-encoding"))) {
    int i;

    for (i = 0; i < 256; i++)
      info->map[i] = i;
    info->data = NULL;
    info->convert = NULL;
    info->release = dummy_release;
    return XML_STATUS_OK;
  }
  return XML_STATUS_ERROR;
}

/* External Entity Handlers */

int XMLCALL
external_entity_optioner(XML_Parser parser, const XML_Char *context,
                         const XML_Char *base, const XML_Char *systemId,
                         const XML_Char *publicId) {
  ExtOption *options = (ExtOption *)XML_GetUserData(parser);
  XML_Parser ext_parser;

  UNUSED_P(base);
  UNUSED_P(publicId);
  while (options->parse_text != NULL) {
    if (! xcstrcmp(systemId, options->system_id)) {
      enum XML_Status rc;
      ext_parser = XML_ExternalEntityParserCreate(parser, context, NULL);
      if (ext_parser == NULL)
        return XML_STATUS_ERROR;
      rc = _XML_Parse_SINGLE_BYTES(ext_parser, options->parse_text,
                                   (int)strlen(options->parse_text), XML_TRUE);
      XML_ParserFree(ext_parser);
      return rc;
    }
    options++;
  }
  fail("No suitable option found");
  return XML_STATUS_ERROR;
}

int XMLCALL
external_entity_loader(XML_Parser parser, const XML_Char *context,
                       const XML_Char *base, const XML_Char *systemId,
                       const XML_Char *publicId) {
  ExtTest *test_data = (ExtTest *)XML_GetUserData(parser);
  XML_Parser extparser;

  UNUSED_P(base);
  UNUSED_P(systemId);
  UNUSED_P(publicId);
  extparser = XML_ExternalEntityParserCreate(parser, context, NULL);
  if (extparser == NULL)
    fail("Could not create external entity parser.");
  if (test_data->encoding != NULL) {
    if (! XML_SetEncoding(extparser, test_data->encoding))
      fail("XML_SetEncoding() ignored for external entity");
  }
  if (_XML_Parse_SINGLE_BYTES(extparser, test_data->parse_text,
                              (int)strlen(test_data->parse_text), XML_TRUE)
      == XML_STATUS_ERROR) {
    xml_failure(extparser);
    return XML_STATUS_ERROR;
  }
  XML_ParserFree(extparser);
  return XML_STATUS_OK;
}

int XMLCALL
external_entity_faulter(XML_Parser parser, const XML_Char *context,
                        const XML_Char *base, const XML_Char *systemId,
                        const XML_Char *publicId) {
  XML_Parser ext_parser;
  ExtFaults *fault = (ExtFaults *)XML_GetUserData(parser);

  UNUSED_P(base);
  UNUSED_P(systemId);
  UNUSED_P(publicId);
  ext_parser = XML_ExternalEntityParserCreate(parser, context, NULL);
  if (ext_parser == NULL)
    fail("Could not create external entity parser");
  if (fault->encoding != NULL) {
    if (! XML_SetEncoding(ext_parser, fault->encoding))
      fail("XML_SetEncoding failed");
  }
  if (_XML_Parse_SINGLE_BYTES(ext_parser, fault->parse_text,
                              (int)strlen(fault->parse_text), XML_TRUE)
      != XML_STATUS_ERROR)
    fail(fault->fail_text);
  if (XML_GetErrorCode(ext_parser) != fault->error)
    xml_failure(ext_parser);

  XML_ParserFree(ext_parser);
  return XML_STATUS_ERROR;
}

int XMLCALL
external_entity_null_loader(XML_Parser parser, const XML_Char *context,
                            const XML_Char *base, const XML_Char *systemId,
                            const XML_Char *publicId) {
  UNUSED_P(parser);
  UNUSED_P(context);
  UNUSED_P(base);
  UNUSED_P(systemId);
  UNUSED_P(publicId);
  return XML_STATUS_OK;
}

/* NotStandalone handlers */

int XMLCALL
reject_not_standalone_handler(void *userData) {
  UNUSED_P(userData);
  return XML_STATUS_ERROR;
}

int XMLCALL
accept_not_standalone_handler(void *userData) {
  UNUSED_P(userData);
  return XML_STATUS_OK;
}

/* Attribute List handlers */
void XMLCALL
verify_attlist_decl_handler(void *userData, const XML_Char *element_name,
                            const XML_Char *attr_name,
                            const XML_Char *attr_type,
                            const XML_Char *default_value, int is_required) {
  AttTest *at = (AttTest *)userData;

  if (xcstrcmp(element_name, at->element_name))
    fail("Unexpected element name in attribute declaration");
  if (xcstrcmp(attr_name, at->attr_name))
    fail("Unexpected attribute name in attribute declaration");
  if (xcstrcmp(attr_type, at->attr_type))
    fail("Unexpected attribute type in attribute declaration");
  if ((default_value == NULL && at->default_value != NULL)
      || (default_value != NULL && at->default_value == NULL)
      || (default_value != NULL && xcstrcmp(default_value, at->default_value)))
    fail("Unexpected default value in attribute declaration");
  if (is_required != at->is_required)
    fail("Requirement mismatch in attribute declaration");
}

/* Character Data handlers */

void XMLCALL
clearing_aborting_character_handler(void *userData, const XML_Char *s,
                                    int len) {
  UNUSED_P(userData);
  UNUSED_P(s);
  UNUSED_P(len);
  XML_StopParser(g_parser, g_resumable);
  XML_SetCharacterDataHandler(g_parser, NULL);
}

void XMLCALL
parser_stop_character_handler(void *userData, const XML_Char *s, int len) {
  UNUSED_P(userData);
  UNUSED_P(s);
  UNUSED_P(len);
  XML_StopParser(g_parser, g_resumable);
  XML_SetCharacterDataHandler(g_parser, NULL);
  if (! g_resumable) {
    /* Check that aborting an aborted parser is faulted */
    if (XML_StopParser(g_parser, XML_FALSE) != XML_STATUS_ERROR)
      fail("Aborting aborted parser not faulted");
    if (XML_GetErrorCode(g_parser) != XML_ERROR_FINISHED)
      xml_failure(g_parser);
  } else if (g_abortable) {
    /* Check that aborting a suspended parser works */
    if (XML_StopParser(g_parser, XML_FALSE) == XML_STATUS_ERROR)
      xml_failure(g_parser);
  } else {
    /* Check that suspending a suspended parser works */
    if (XML_StopParser(g_parser, XML_TRUE) != XML_STATUS_ERROR)
      fail("Suspending suspended parser not faulted");
    if (XML_GetErrorCode(g_parser) != XML_ERROR_SUSPENDED)
      xml_failure(g_parser);
  }
}

/* Handlers that record their invocation by single characters */

void XMLCALL
record_default_handler(void *userData, const XML_Char *s, int len) {
  UNUSED_P(s);
  UNUSED_P(len);
  CharData_AppendXMLChars((CharData *)userData, XCS("D"), 1);
}

void XMLCALL
record_cdata_handler(void *userData, const XML_Char *s, int len) {
  UNUSED_P(s);
  UNUSED_P(len);
  CharData_AppendXMLChars((CharData *)userData, XCS("C"), 1);
  XML_DefaultCurrent(g_parser);
}

void XMLCALL
record_cdata_nodefault_handler(void *userData, const XML_Char *s, int len) {
  UNUSED_P(s);
  UNUSED_P(len);
  CharData_AppendXMLChars((CharData *)userData, XCS("c"), 1);
}

void XMLCALL
record_skip_handler(void *userData, const XML_Char *entityName,
                    int is_parameter_entity) {
  UNUSED_P(entityName);
  CharData_AppendXMLChars((CharData *)userData,
                          is_parameter_entity ? XCS("E") : XCS("e"), 1);
}

/* Entity Declaration Handlers */
static const XML_Char *entity_name_to_match = NULL;
static const XML_Char *entity_value_to_match = NULL;
static int entity_match_flag = ENTITY_MATCH_NOT_FOUND;

void XMLCALL
param_entity_match_handler(void *userData, const XML_Char *entityName,
                           int is_parameter_entity, const XML_Char *value,
                           int value_length, const XML_Char *base,
                           const XML_Char *systemId, const XML_Char *publicId,
                           const XML_Char *notationName) {
  UNUSED_P(userData);
  UNUSED_P(base);
  UNUSED_P(systemId);
  UNUSED_P(publicId);
  UNUSED_P(notationName);
  if (! is_parameter_entity || entity_name_to_match == NULL
      || entity_value_to_match == NULL) {
    return;
  }
  if (! xcstrcmp(entityName, entity_name_to_match)) {
    /* The cast here is safe because we control the horizontal and
     * the vertical, and we therefore know our strings are never
     * going to overflow an int.
     */
    if (value_length != (int)xcstrlen(entity_value_to_match)
        || xcstrncmp(value, entity_value_to_match, value_length)) {
      entity_match_flag = ENTITY_MATCH_FAIL;
    } else {
      entity_match_flag = ENTITY_MATCH_SUCCESS;
    }
  }
  /* Else leave the match flag alone */
}

void
param_entity_match_init(const XML_Char *name, const XML_Char *value) {
  entity_name_to_match = name;
  entity_value_to_match = value;
  entity_match_flag = ENTITY_MATCH_NOT_FOUND;
}

int
get_param_entity_match_flag(void) {
  return entity_match_flag;
}
