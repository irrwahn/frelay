/*
 * statcodes.c
 *
 * Copyright 2016 Urban Wallasch <irrwahn35@freenet.de>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer
 *   in the documentation and/or other materials provided with the
 *   distribution.
 * * Neither the name of the copyright holder nor the names of its
 *   contributors may be used to endorse or promote products derived
 *   from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *
 */


#include "statcodes.h"


const char *sc_msgstr( enum SC_ENUM sc )
{
    switch (sc )
    {
    /*100*/case SC_CONTINUE:                        return "Continue"; break;
    /*101*/case SC_SWITCHING_PROTOCOLS:             return "Switching Protocols"; break;
    /*102*/case SC_PROCESSING:                      return "Processing"; break;

    /*200*/case SC_OK:                              return "OK"; break;
    /*201*/case SC_CREATED:                         return "Created"; break;
    /*202*/case SC_ACCEPTED:                        return "Accepted"; break;
    /*203*/case SC_NON_AUTHORITATIVE_INFORMATION:   return "Non-Authoritative Information"; break;
    /*204*/case SC_NO_CONTENT:                      return "No Content"; break;
    /*205*/case SC_RESET_CONTENT:                   return "Reset Content"; break;
    /*206*/case SC_PARTIAL_CONTENT:                 return "Partial Content"; break;
    /*207*/case SC_MULTI_STATUS:                    return "Multi-Status"; break;
    /*208*/case SC_ALREADY_REPORTED:                return "Already Reported"; break;
    /*226*/case SC_IM_USED:                         return "IM Used"; break;

    /*300*/case SC_MULTIPLE_CHOICES:                return "Multiple Choices"; break;
    /*301*/case SC_MOVED_PERMANENTLY:               return "Moved Permanently"; break;
    /*302*/case SC_FOUND:                           return "Found"; break;
    /*303*/case SC_SEE_OTHER:                       return "See Other"; break;
    /*304*/case SC_NOT_MODIFIED:                    return "Not Modified"; break;
    /*305*/case SC_USE_PROXY:                       return "Use Proxy"; break;
    /*307*/case SC_TEMPORARY_REDIRECT:              return "Temporary Redirect"; break;
    /*308*/case SC_PERMANENT_REDIRECT:              return "Permanent Redirect"; break;

    /*400*/case SC_BAD_REQUEST:                     return "Bad Request"; break;
    /*401*/case SC_UNAUTHORIZED:                    return "Unauthorized"; break;
    /*402*/case SC_PAYMENT_REQUIRED:                return "Payment Required"; break;
    /*403*/case SC_FORBIDDEN:                       return "Forbidden"; break;
    /*404*/case SC_NOT_FOUND:                       return "Not Found"; break;
    /*405*/case SC_METHOD_NOT_ALLOWED:              return "Method Not Allowed"; break;
    /*406*/case SC_NOT_ACCEPTABLE:                  return "Not Acceptable"; break;
    /*407*/case SC_PROXY_AUTHENTICATION_REQUIRED:   return "Proxy Authentication Required"; break;
    /*408*/case SC_REQUEST_TIMEOUT:                 return "Request Timeout"; break;
    /*409*/case SC_CONFLICT:                        return "Conflict"; break;
    /*410*/case SC_GONE:                            return "Gone"; break;
    /*411*/case SC_LENGTH_REQUIRED:                 return "Length Required"; break;
    /*412*/case SC_PRECONDITION_FAILED:             return "Precondition Failed"; break;
    /*413*/case SC_PAYLOAD_TOO_LARGE:               return "Payload Too Large"; break;
    /*414*/case SC_URI_TOO_LONG:                    return "URI Too Long"; break;
    /*415*/case SC_UNSUPPORTED_MEDIA_TYPE:          return "Unsupported Media Type"; break;
    /*416*/case SC_RANGE_NOT_SATISFIABLE:           return "Range Not Satisfiable"; break;
    /*417*/case SC_EXPECTATION_FAILED:              return "Expectation Failed"; break;
    /*418*/case SC_I_AM_A_TEAPOT:                   return "I'm a teapot"; break;
    /*421*/case SC_MISDIRECTED_REQUEST:             return "Misdirected Request"; break;
    /*422*/case SC_UNPROCESSABLE_ENTITY:            return "Unprocessable Entity"; break;
    /*423*/case SC_LOCKED:                          return "Locked"; break;
    /*424*/case SC_FAILED_DEPENDENCY:               return "Failed Dependency"; break;
    /*426*/case SC_UPGRADE_REQUIRED:                return "Upgrade Required"; break;
    /*428*/case SC_PRECONDITION_REQUIRED:           return "Precondition Required"; break;
    /*429*/case SC_TOO_MANY_REQUESTS:               return "Too Many Requests"; break;
    /*431*/case SC_REQUEST_HEADER_FIELDS_TOO_LARGE: return "Request Header Fields Too Large"; break;
    /*451*/case SC_UNAVAILABLE_FOR_LEGAL_REASONS:   return "Unavailable For Legal Reasons"; break;

    /*500*/case SC_INTERNAL_SERVER_ERROR:           return "Internal Server Error"; break;
    /*501*/case SC_NOT_IMPLEMENTED:                 return "Not Implemented"; break;
    /*502*/case SC_BAD_GATEWAY:                     return "Bad Gateway"; break;
    /*503*/case SC_SERVICE_UNAVAILABLE:             return "Service Unavailable"; break;
    /*504*/case SC_GATEWAY_TIMEOUT:                 return "Gateway Timeout"; break;
    /*505*/case SC_HTTP_VERSION_NOT_SUPPORTED:      return "HTTP Version Not Supported"; break;
    /*506*/case SC_VARIANT_ALSO_NEGOTIATES:         return "Variant Also Negotiates"; break;
    /*507*/case SC_INSUFFICIENT_STORAGE:            return "Insufficient Storage"; break;
    /*508*/case SC_LOOP_DETECTED:                   return "Loop Detected"; break;
    /*510*/case SC_NOT_EXTENDED:                    return "Not Extended"; break;
    /*511*/case SC_NETWORK_AUTHENTICATION_REQUIRED: return "Network Authentication Required"; break;

    /*---*/default: break;
    }
    return "Unknown Error";
}


/* EOF */
