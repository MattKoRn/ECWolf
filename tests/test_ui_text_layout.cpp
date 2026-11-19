#include "../src/ui_text_layout.h"
#include <cassert>
#include <string>
struct FixedFont { int operator()(const std::string &s) const { return static_cast<int>(s.size())*8; } };
int main()
{
	FixedFont width;
	assert(UITextLayout::FitLine("Short",48,width)=="Short");
	assert(UITextLayout::FitLine("Long label",48,width)=="Lon...");
	assert(UITextLayout::FitLine("ABC",16,width)=="..");
	assert(UITextLayout::FitLine("abc",0,width).empty());
	assert(UITextLayout::FitLines("Long label\nOK",48,width)=="Lon...\nOK");
	assert(UITextLayout::Wrap("one two three",56,3,width)=="one two\nthree");
	assert(UITextLayout::Wrap("abcdefgh",32,3,width)=="abcd\nefgh");
	assert(UITextLayout::Wrap("one two three four",56,2,width)=="one two\nthre...");
	assert(UITextLayout::Wrap("line1\nline2",80,3,width)=="line1\nline2");
}
