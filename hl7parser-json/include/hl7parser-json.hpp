#pragma once

#include <hl7parser/buffer.h>
#include <hl7parser/defs.h>
#include <hl7parser/element.h>
#include <hl7parser/message.h>
#include <hl7parser/parser.h>
#include <hl7parser/token.h>
#include <hl7parser/settings.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <sstream>
#include <exception>
#include "xml2json.hpp"

using namespace std;

namespace hl7parserJson{
  enum class Format { json, xml };

  template<Format format>
  class hl7parser {
  public:
    string parseHeader(const string& input) {
      return parse(input, true);
    }

    string parse(const string& input, const bool _headerOnly = false) {
      headerOnly = _headerOnly;
      string resp;
      int             rc = 0;
      HL7_Settings    settings;
      HL7_Buffer      input_buffer;
      HL7_Buffer      output_buffer;
      HL7_Allocator   allocator;
      HL7_Message     message;
      HL7_Parser      parser;
      char            *data;
      size_t message_length  = input.size();

      hl7_settings_init(&settings);

      /* Initialize the buffer excluding the null terminator. */
      auto cstr = const_cast<char*>(input.c_str());      
      hl7_buffer_init(&input_buffer, cstr, message_length);
      hl7_buffer_move_wr_ptr(&input_buffer, message_length);
      
      /* Initialize the message */
      hl7_allocator_init(&allocator, malloc, free);
      hl7_message_init(&message, &settings, &allocator);

      /* Initialize the parser. */
      hl7_parser_init(&parser, &settings);

      rc = hl7_parser_read(&parser, &message, &input_buffer);
      
      if (rc == 0) {
        resp = print_message(&message);        
      }

      /* Clean up */
      hl7_parser_fini(&parser);
      hl7_message_fini(&message);
      hl7_allocator_fini(&allocator);
      hl7_buffer_fini(&input_buffer);
      hl7_settings_fini(&settings);

      if (rc == 0) return resp;

      throw "Error processing message";
    }
  private:
    stringstream ostr;
    bool headerOnly;
    int segmentIndex = -1;
    
    void encode(std::string& data) {
      std::string buffer;
      buffer.reserve(data.size());
      for (size_t pos = 0; pos != data.size(); ++pos) {
        switch (data[pos]) {
        case '&':  buffer.append("&amp;");       break;
        case '\"': buffer.append("&quot;");      break;
        case '\'': buffer.append("&apos;");      break;
        case '<':  buffer.append("&lt;");        break;
        case '>':  buffer.append("&gt;");        break;
        default:   buffer.append(&data[pos], 1); break;
        }
      }
      data.swap(buffer);
    }

    char *element_type_name(const HL7_Element_Type element_type) {
      static char *ELEMENT_TYPE_NAME[HL7_ELEMENT_TYPE_COUNT] = {
        const_cast<char*>("Subcomponent"),
        const_cast<char*>("Component"),
        const_cast<char*>("Repetition"),
        const_cast<char*>("Field"),
        const_cast<char*>("Segment")
      };

      return (element_type < HL7_ELEMENT_TYPE_COUNT ? ELEMENT_TYPE_NAME[(int)element_type] : const_cast<char*>("Unknown"));
    }

    void print_node(HL7_Node *node, HL7_Element_Type element_type) {
      if (node != 0) {
        const char          *type_name;
        size_t              characters_length;

        if (headerOnly && segmentIndex > 0)
          return;

        do {
          type_name = element_type_name(element_type);
          
          if (element_type == HL7_ELEMENT_SEGMENT) {
            ++segmentIndex;
          }
          
          ostr << "<" << type_name << ">";

          if (node->element.length > 0) {
            characters_length = node->element.length;

            vector<char> tmp(node->element.value, node->element.value + characters_length + 1);
            auto str = string(tmp.begin(), tmp.end() - 1);
            
            encode(str);
            
            ostr << str;            
          } else if (node->children == 0) {
            ostr << "";
          }

          if (node->children != 0) {
            print_node(node->children, hl7_child_type(element_type));
          }

          ostr << "</" << type_name << ">\n";

          node = node->sibling;

        } while (node != 0);
      }
    }

    string print_message(HL7_Message *message) {
      if (message != 0) {
        ostr << "<Message>\n";
        print_node(message->head, HL7_ELEMENT_SEGMENT);
        ostr << "</Message>\n";

        string raw = ostr.str();
        
        if (format == Format::json) return xml2json(raw.c_str());
        if (format == Format::xml) return raw;
      }
    }
  };
}