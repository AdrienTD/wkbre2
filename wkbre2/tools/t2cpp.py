
import sys, os

class TokenParser:
    def __init__(self, text):
        self.cursor = 0
        self.text = text
    def __next__(self):
        quoted = False
        
        try:
            while self.text[self.cursor].isspace():
                self.cursor += 1
        except IndexError:
            raise StopIteration
        start = self.cursor
        if self.text[self.cursor] == '\"':
            quoted = True
            self.cursor += 1

        try:
            if quoted:
                while self.text[self.cursor] != '\"':
                    self.cursor += 1
            else:
                while not self.text[self.cursor].isspace():
                    self.cursor += 1
        except IndexError:
            return self.text[start:]
        if quoted:
            self.cursor += 1
        end = self.cursor

        return self.text[start:end]

with open('../tags.txt', 'r') as f, open('../tags.cpp', 'w') as out_c, open('../tags.h', 'w') as out_h:

    print("""
#pragma once
#include "util/TagDict.h"

namespace Tags
{
""", file=out_h)

    print("""#include "tags.h" """, file=out_c)
    
    words = f.read()
    wit = TokenParser(words)

    while True:
        # Name for set
        try:
            word = next(wit)
            while word not in ('{', '|'):
                setname = word
                word = next(wit)
        except StopIteration:
            print('// Correct end')
            break

        # Additional data per tag
        dattypes = []
        if word == '|':
            while True:
                word = next(wit)
                if word == '{':
                    break
                dattypes.append(word)
        adddata = [[] for i in dattypes]

        # Tags
        tags = []
        tag = ''
        while True:
            tag = next(wit)
            if tag == '}':
                break
            ad = []
            for i in range(len(dattypes)):
                ad.append(next(wit))
            tags.append((tag,ad))
        tags.sort()

##        print('enum %s {' % setname)
##        print('    %s = 0,' % tags[0][0])
##        for tag,ad in tags[1:]:
##            print('    %s,' % tag)
##        print('};')

        cnt = 0
        for tag,ad in tags:
            print('const int %s%s = %i;' % (setname,tag,cnt), file=out_h)
            cnt += 1
        print('const int %sCOUNT = %i;' % (setname,cnt), file=out_h)

        #print('extern const char * Tokens_%s[%i];' % (setname,len(tags)), file=out_c)
        #print('const char * Tokens_%s[%i] = {' % (setname,len(tags)), file=out_c)
        print('extern TagDict<%i> %stagDict;\n' % (len(tags),setname), file=out_h)
        print('TagDict<%i> Tags::%stagDict({' % (len(tags),setname), file=out_c)
        for tag,ad in tags:
            print('    \"%s\",' % tag, file=out_c)
            cnt += 1
        print('});\n', file=out_c)

    print('} // end namespace', file=out_h)
    
