/*
 *   splitstream.c
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

#include "splitstream_private.h"
#include <string.h>

static void AppendDoc(SplitstreamState* state, SplitstreamDocument* dest, const void* ptr, size_t length);

const static int SPLITSTREAM_STATE_FLAG_DID_RETURN_DOCUMENT = 8;
const static int SPLITSTREAM_STATE_FLAG_FILE_EOF = 16;

struct mempool* mempool_New(void);
void mempool_Destroy(struct mempool* pool, int check);
void* mempool_Alloc(struct mempool* pool, size_t size);
void* mempool_ReAlloc(struct mempool* pool, void* ptr, size_t oldSize, size_t newSize);
void mempool_Free(struct mempool* pool, void* ptr, size_t size);

SplitstreamDocument SPLITSTREAM_API SplitstreamGetNextDocument(SplitstreamState* s, size_t max, const char* buf, size_t len, SplitstreamScanner scan) {
    size_t start = (size_t)-1, end;
    int didSetStart = 0;
    SplitstreamDocument doc = { NULL, 0 };
    SplitstreamDocument rescanDoc = { NULL, 0 };

    if(s->state == State_Rescan) {
        rescanDoc = s->doc;
        s->doc.buffer = NULL;
        s->doc.length = 0;
        if(buf && len) {
            AppendDoc(s, &rescanDoc, buf, len);
        }
        s->state = State_Init;
        buf = rescanDoc.buffer;
        len = rescanDoc.length;
    }
    end = scan(s, buf, len, &start);
    if(start != (size_t)-1) didSetStart = 1;
    else start = 0;

    if(end > 0) { /* Did find a document */
        doc = s->doc;
        s->doc.buffer = NULL;
        s->doc.length = 0;
        if(buf && len) {
            AppendDoc(s, &doc, buf + start, end - start);
        }
        s->state = (end < len) ? State_Rescan : State_Init;
        start = end;
    }
    if(s->state != State_Init && start < len) {
        if(didSetStart) {
            SplitstreamDocumentFree(s, &s->doc);
        } else if(s->doc.length + len - start > max) {
            // If document was too large, discard it
            SplitstreamDocumentFree(s, &s->doc);
            s->state = State_Init;
        }
        if(buf && len) {
            AppendDoc(s, &s->doc, buf + start, len - start);
        }
    }
    SplitstreamDocumentFree(s, &rescanDoc);
    return doc;
}

SplitstreamDocument SPLITSTREAM_API SplitstreamGetNextDocumentFromFile(SplitstreamState* s, char* buf, size_t bufferSize, size_t max, FILE* file, SplitstreamScanner scanner) {
    if(!bufferSize || bufferSize > 1024*1024*1024) bufferSize = 1024;

    if(s->flags & SPLITSTREAM_STATE_FLAG_DID_RETURN_DOCUMENT) {
        SplitstreamDocument doc = SplitstreamGetNextDocument(s, max, NULL, 0, scanner);
        if(doc.buffer) {
            s->flags |= SPLITSTREAM_STATE_FLAG_DID_RETURN_DOCUMENT;
            return doc;
        }
    }

    while(file) {
        size_t len = fread(buf, 1, bufferSize, file);
        if(len == 0) {
        	s->flags |= SPLITSTREAM_STATE_FLAG_FILE_EOF;
            SplitstreamDocument doc = {NULL, 0};
            return doc;
        }

        SplitstreamDocument doc = SplitstreamGetNextDocument(s, max, buf, len, scanner);
        if(doc.buffer) {
            s->flags |= SPLITSTREAM_STATE_FLAG_DID_RETURN_DOCUMENT;
            return doc;
        }
    }
    s->flags &= ~SPLITSTREAM_STATE_FLAG_DID_RETURN_DOCUMENT;

    SplitstreamDocument doc = {NULL, 0};
    return doc;

}

void SPLITSTREAM_API SplitstreamDocumentFree(SplitstreamState* state, SplitstreamDocument* doc) {
    if(doc->buffer) {
    	if(state && state->mempool)
    		mempool_Free(state->mempool, (void*)doc->buffer, doc->length);
    }
    doc->buffer = NULL;
    doc->length = 0;
}

void SPLITSTREAM_API SplitstreamInit(SplitstreamState* state) {
    memset(state, 0, sizeof(SplitstreamState));
}

void SPLITSTREAM_API SplitstreamInitDepth(SplitstreamState* state, int startDepth) {
    SplitstreamInit(state);
    if(startDepth > 0) state->startDepth = startDepth;
}

void SPLITSTREAM_API SplitstreamFree(SplitstreamState* state) {
    SplitstreamDocumentFree(state, &state->doc);
    if(state->mempool) mempool_Destroy(state->mempool, 1);
    state->mempool = NULL;
}


static void AppendDoc(SplitstreamState* state, SplitstreamDocument* dest, const void* ptr, size_t length) {
    if(!length) return;

	if(!state->mempool) state->mempool = mempool_New();
    size_t prevLength = 0;
    if(!dest->buffer) {
        dest->length = length;
        dest->buffer = mempool_Alloc(state->mempool, dest->length);
    } else {
        prevLength = dest->length;
        dest->length += length;
        dest->buffer = mempool_ReAlloc(state->mempool, (void*)dest->buffer, prevLength, dest->length);
    }
    if(!dest->buffer) abort();
    memcpy(((char*)dest->buffer) + prevLength, ptr, length);
}
