#pragma once

#include <string>
#include <vector>
#include <map>
#include <set>

class Gui
{
public:
	Gui() = default;
	~Gui() = default;

	typedef uint32_t Handle;

	typedef Handle Font;
	typedef Handle FontSize;
	typedef Handle Image;

	Font loadFont(const std::string &filename);
	FontSize loadFontSize(Font font, float size);
	Image loadImage(const std::string &filename);

	void init();
	void setText(FontSize fontSize, int posX, int posY, const std::string &text, 
		uint8_t r, uint8_t g, uint8_t b, uint8_t a);
	void setImage(Image img, int posX, int posY, float size);

	void display(int width, int height);

protected:
	struct Vertex
	{
		float x,y,u,v;
		uint8_t r,g,b,a;
	};

	struct RenderInfo
	{
		std::vector<Vertex> vertices;
	};

	virtual void initGraphics(
		int atlasWidth, int atlasHeight, 
		const std::vector<uint8_t> &atlasData)=0;
	virtual void displayGraphics(const RenderInfo &info)=0;

private:
	Handle genHandle();

	struct GlyphInfo
	{
		int x0, y0, x1, y1;
		int x, y, w, h;
	};

	struct FontSizeInfo
	{
		float pixelSize;
		float scale;
		std::map<int, GlyphInfo> glyphInfo;
	};
	struct FontInfo
	{
		std::string filename;
		std::map<FontSize, FontSizeInfo> fontSizeInfo;
		int ascent, descent, lineGap;
		std::map<int, int> advanceWidth;
		std::map<int, int> leftSideBearing;
		std::map<std::pair<int,int>, int> kernAdvance;
		std::vector<int> codepointGlyphs;
		std::set<int> allGlyphs;
	};

	std::map<Font, FontInfo> _fontInfo;
	std::map<FontSize, Font> _fontSizes;

	struct TextRenderInfo
	{
		Font font;
		FontSize fontSize;
		int posX, posY;
		std::string text;
		uint8_t r,g,b,a;
	};

	std::vector<TextRenderInfo> _textRenderInfo;
	int _atlasWidth, _atlasHeight;
};