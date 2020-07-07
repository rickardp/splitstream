/*
 *   splitstream_xml.c
 *   splitstream - Stream object splitter
 *
 *   Copyright © 2015 Rickard Lyrenius
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 */

#include <splitstream_private.h>

const int COUNTER_DASH = 0;
const int COUNTER_CLOSING_BRACKET = 1;

size_t SplitstreamXMLScanner(SplitstreamState* s, const char* buf, size_t len, size_t* start) {
    int dashCounter = s->counter[COUNTER_DASH], bracketCounter = s->counter[COUNTER_CLOSING_BRACKET];
    SplitstreamTokenizerState state = s->state;
    const char* end = buf + len, *cp = buf;
    
    #define LOOP_BEGIN \
	    for(; cp != end; ++cp) { \
    	    char c = *cp; \
	        switch(c) {
	        
	#define LOOP_END \
			} \
	        s->last = c; \
		} \
		goto h_End;
	
	#define TRANSITION(x) \
		state = State_##x; \
		++cp; \
		goto h_##x;
	
	#define CHECK_END \
		if(s->depth == s->startDepth) { \
			s->last = c; \
			s->state = state; \
			s->counter[COUNTER_DASH] = s->counter[COUNTER_CLOSING_BRACKET] = 0; \
			return (cp - buf + 1); \
		} else { \
			TRANSITION(Document) \
		}
	
		switch(state) {
			case State_Init:
				goto h_Init;
			case State_Document:
				goto h_Document;
			case State_ElementOrComment:
				goto h_ElementOrComment;
			case State_CommentOrInstruction:
				goto h_CommentOrInstruction;
			case State_BeginElement:
				goto h_BeginElement;
			case State_EndElement:
				goto h_EndElement;
			case State_Comment:
				goto h_Comment;
			case State_Instruction:
				goto h_Instruction;
			case State_Cdata:
				goto h_Cdata;
			default:
				abort();
		}
		
	h_Init:
	h_Document:
		LOOP_BEGIN
		case '<': 
			if(state == State_Init || (s->depth == s->startDepth && s->startDepth > 0)) {
				*start = (cp - buf);
			}
			TRANSITION(ElementOrComment)
			break;
		LOOP_END
	
	h_ElementOrComment:
		LOOP_BEGIN
		default:
			TRANSITION(BeginElement)
			break;
		case '>':
			state = State_Document;
			if(s->last != '/') ++s->depth;
			else { CHECK_END }
			TRANSITION(Document)
			break;
		case '/':
			TRANSITION(EndElement)
			break;
		case '?':
			TRANSITION(Instruction)
			break;
		case '!':
			TRANSITION(CommentOrInstruction)
			break;
		LOOP_END
	
	h_CommentOrInstruction:
		LOOP_BEGIN
		default:
			dashCounter = 0;
			TRANSITION(Instruction)
			break;
		case '-':
			if(dashCounter > 0) {
				dashCounter = 0;
				TRANSITION(Comment)
			}
			++dashCounter;
			break;
        case '>':
			dashCounter = 0;
			TRANSITION(Document)
			break;
		case '[':
			dashCounter = 0;
			TRANSITION(Cdata)
			break;
		LOOP_END
		
	h_BeginElement:
		LOOP_BEGIN
		case '>':
			state = State_Document;
			if(s->last != '/') ++s->depth;
			else { CHECK_END }
			TRANSITION(Document)
			break;
		LOOP_END
		
	h_EndElement:
		LOOP_BEGIN
		case '>':
			--s->depth;
			CHECK_END
			break;
			
		LOOP_END
		
	h_Comment:
		LOOP_BEGIN
		default:
			dashCounter = 0;
			break;
        case '>':
        	if(dashCounter >= 2) {
				dashCounter = 0;
				TRANSITION(Document)
        	}
        	break;
		case '-':
			++dashCounter;
			break;
		LOOP_END
		
	h_Instruction:
		LOOP_BEGIN
        case '>':
			TRANSITION(Document)
			break;
		LOOP_END
		
	h_Cdata:
		LOOP_BEGIN
		default:
			bracketCounter = 0;
			break;
        case '>':
        	if(bracketCounter >= 2) {
				bracketCounter = 0;
				TRANSITION(Document)
        	}
			bracketCounter = 0;
        	break;
		case ']':
			++bracketCounter;
			break;
		LOOP_END
        
    h_End:
		s->state = state;
		s->counter[COUNTER_DASH] = dashCounter;
		s->counter[COUNTER_CLOSING_BRACKET] = bracketCounter;
		return 0;
}
