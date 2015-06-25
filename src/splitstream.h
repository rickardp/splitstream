/*
 *   splitstream.h
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

#ifndef __SPLITSTREAM_H_INC
#define __SPLITSTREAM_H_INC

#include <stdlib.h>
#include <stdio.h>

#ifndef __SPLITSTREAM_PRIVATE_H_INC
typedef int SplitstreamTokenizerState;
#endif

typedef struct {
    const char* buffer;
    size_t length;
} SplitstreamDocument;

typedef struct {
    int startDepth;
    int depth;
    int counter[4];
    char last;
    int flags;
    SplitstreamTokenizerState state;
    SplitstreamDocument doc;
    struct mempool* mempool;
} SplitstreamState;

#ifndef SPLITSTREAM_API
#define SPLITSTREAM_API
#endif

/* You can implement your own serialization formats by providing a custom scanner. */
typedef size_t (*SplitstreamScanner)(SplitstreamState* ptr, const char* buf, size_t len, size_t* start);

/* Scanners. Send function pointer as last parameter of SplitstreamGetNextDocument. */
size_t SPLITSTREAM_API SplitstreamXMLScanner(SplitstreamState* s, const char* buf, size_t len, size_t* start);
size_t SPLITSTREAM_API SplitstreamJSONScanner(SplitstreamState* s, const char* buf, size_t len, size_t* start);

void SPLITSTREAM_API SplitstreamDocumentFree(SplitstreamState* state, SplitstreamDocument* doc);
void SPLITSTREAM_API SplitstreamInit(SplitstreamState* state);
void SPLITSTREAM_API SplitstreamInitDepth(SplitstreamState* state, int startDepth);
void SPLITSTREAM_API SplitstreamFree(SplitstreamState* state);
SplitstreamDocument SPLITSTREAM_API SplitstreamGetNextDocument(SplitstreamState* state, size_t max, const char* buf, size_t len, SplitstreamScanner scan);
SplitstreamDocument SPLITSTREAM_API SplitstreamGetNextDocumentFromFile(SplitstreamState* s, char* buf, size_t bufferSize, size_t max, FILE* file, SplitstreamScanner scanner);

#endif /* __SPLITSTREAM_H_INC */