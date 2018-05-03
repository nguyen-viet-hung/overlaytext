#ifndef CV_RENDER_TEXT_H__
#define CV_RENDER_TEXT_H__

// FreeType headers
#include <ft2build.h>
#include FT_FREETYPE_H

#include <cstring>

// OpenCV headers
#include <opencv2/core/core.hpp>

class CVRenderText
{
protected:
	FT_Library mLibrary;
	FT_Face mFace;
	bool mInitialized;
	std::string mFontName;
public:
	typedef enum {
		LEFT_MARGIN,
		RIGHT_MARGIN,
		TOP_MARGIN,
		BOTTOM_MARGIN,
		CENTER_MARGIN
	} Justify;

	CVRenderText();
	virtual ~CVRenderText();

	int setFont(const char* path_to_font);

	int renderText(cv::Mat &dstImg, cv::Point pos, const wchar_t* text, size_t textSize, Justify xMargin = CENTER_MARGIN, Justify yMargin = CENTER_MARGIN, 
		cv::Scalar textColor = cv::Scalar::all(255), bool hasBackgrnd = true, cv::Scalar bgrColor = cv::Scalar::all(0), double bgrOpacity = 0.0);

	int renderText(cv::Mat &dstImg, cv::Point pos, const char* text, size_t textSize, Justify xMargin = CENTER_MARGIN, Justify yMargin = CENTER_MARGIN, 
		cv::Scalar textColor = cv::Scalar::all(255), bool hasBackgrnd = true, cv::Scalar bgrColor = cv::Scalar::all(0), double bgrOpacity = 0.0);
};

#endif//CV_RENDER_TEXT_H__