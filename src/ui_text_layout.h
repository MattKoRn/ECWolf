#ifndef ECWOLF_UI_TEXT_LAYOUT_H
#define ECWOLF_UI_TEXT_LAYOUT_H
#include <cctype>
#include <string>
#include <vector>

namespace UITextLayout {
// The width callback uses the exact font metrics from the drawing routine.
template<class Width>
std::string FitLine(const std::string &source, int maxWidth, Width width)
{
	if(maxWidth <= 0) return "";
	if(width(source) <= maxWidth) return source;
	std::string dots = "...";
	while(!dots.empty() && width(dots) > maxWidth) dots.erase(dots.size()-1);
	if(dots.empty()) return "";
	std::string result;
	for(size_t i=0; i<source.size(); ++i)
	{
		if(width(result + source[i] + dots) > maxWidth) break;
		result += source[i];
	}
	return result + dots;
}

template<class Width>
std::string FitLines(const std::string &source, int maxWidth, Width width)
{
	std::string result;
	size_t start=0;
	for(;;)
	{
		const size_t end = source.find('\n', start);
		if(start) result += '\n';
		result += FitLine(source.substr(start, end == std::string::npos ? end : end-start), maxWidth, width);
		if(end == std::string::npos) return result;
		start = end+1;
	}
}

template<class Width>
std::string Wrap(const std::string &source, int maxWidth, int maxLines, Width width)
{
	if(maxWidth <= 0 || maxLines <= 0) return "";
	std::vector<std::string> lines;
	std::string current;
	for(size_t i=0;i<source.size();)
	{
		unsigned char c = static_cast<unsigned char>(source[i]);
		if(c=='\r') { ++i; continue; }
		if(c=='\n') { lines.push_back(current); current.clear(); ++i; continue; }
		if(std::isspace(c))
		{
			if(!current.empty() && current[current.size()-1]!=' ' && width(current+" ")<=maxWidth) current += ' ';
			++i; continue;
		}
		size_t end=i;
		while(end<source.size() && !std::isspace(static_cast<unsigned char>(source[end]))) ++end;
		std::string word=source.substr(i,end-i); i=end;
		if(width(current+word)<=maxWidth) {current+=word; continue;}
		if(!current.empty())
		{
			while(!current.empty() && current[current.size()-1]==' ') current.erase(current.size()-1);
			lines.push_back(current); current.clear();
		}
		while(!word.empty() && width(word)>maxWidth)
		{
			size_t n=0;
			while(n<word.size() && width(word.substr(0,n+1))<=maxWidth) ++n;
			if(n==0) {word.erase(0,1); continue;}
			lines.push_back(word.substr(0,n)); word.erase(0,n);
		}
		current=word;
	}
	lines.push_back(current);
	if(static_cast<int>(lines.size())>maxLines)
	{
		lines.resize(maxLines);
		lines.back()=FitLine(lines.back()+"...",maxWidth,width);
	}
	std::string result;
	for(size_t i=0;i<lines.size();++i) {if(i) result+='\n';result+=lines[i];}
	return result;
}
}
#endif
