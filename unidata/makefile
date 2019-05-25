#
# Generate unicode data and code
#

BUILD_CXX ?= g++
BUILD_FLAGS := -ggdb -Wall

.PHONY: all
all: debug unicase unicrng

.PHONY: code
code: debug/gencase debug/gencrng debug/genul

.PHONY: clean
clean: debug
	-rm -R debug/*

unicrng: debug/gencrng debug/genul
	-rm ../src/unicrng.hpp
	@echo "/*\n\
\n\
Foment\n\
\n\
*/\n\
\n\
// Automatically generated unicode character ranges included by charsets.cpp."\
> ../src/unicrng.hpp
	debug/genul DerivedCoreProperties.txt LowerCaseCharRange Lowercase >> ../src/unicrng.hpp
	debug/genul DerivedCoreProperties.txt UpperCaseCharRange Uppercase >> ../src/unicrng.hpp
	debug/gencrng UnicodeData.txt TitleCaseCharRange Lt >> ../src/unicrng.hpp
	debug/genul DerivedCoreProperties.txt LetterCharRange Alphabetic >> ../src/unicrng.hpp
	debug/gencrng UnicodeData.txt DigitCharRange Nd >> ../src/unicrng.hpp
	debug/gencrng UnicodeData.txt WhitespaceCharRange Zs Zl Zp +0009 +000A +000B +000C +000D \
>> ../src/unicrng.hpp
	debug/gencrng UnicodeData.txt PunctuationCharRange Pc Pd Ps Pe Pi Pf Po >> ../src/unicrng.hpp
	debug/gencrng UnicodeData.txt SymbolCharRange Sm Sc Sk So >> ../src/unicrng.hpp
	debug/gencrng UnicodeData.txt BlankCharRange Zs +0009 >> ../src/unicrng.hpp

unicase: debug/gencase
	-rm ../src/unicase.hpp
	@echo "/*\n\
\n\
Foment\n\
\n\
*/\n\
\n\
// Automatically generated unicode data and code included by unicode.cpp.\n" > ../src/unicase.hpp
	debug/gencase UnicodeData.txt Upcase 12 50 >> ../src/unicase.hpp
	debug/gencase UnicodeData.txt Downcase 13 50 >> ../src/unicase.hpp
	debug/gencase CaseFolding.txt Foldcase 2 50 >> ../src/unicase.hpp
	debug/gencase CaseFolding.txt Fullfold 2 20 >> ../src/unicase.hpp
	debug/gencase SpecialCasing.txt Fullup 3 20 >> ../src/unicase.hpp

debug:
	-mkdir debug

debug/%: ./%.cpp
	$(BUILD_CXX) $(BUILD_FLAGS) -o $@ $<
