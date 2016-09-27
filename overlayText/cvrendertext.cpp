#include "cvrendertext.h"
#include <cwchar>
#include <stdint.h>
#include <vector>

#include FT_GLYPH_H

CVRenderText::CVRenderText()
{
	FT_Error error;
	error = FT_Init_FreeType(&mLibrary);

	if (!error)
		mInitialized = true;
	else
		mInitialized = false;

	mFace = NULL;
}

CVRenderText::~CVRenderText()
{
	if (mFace) {
		FT_Done_Face(mFace);
		mFace = NULL;
	}

	if (mLibrary) {
		FT_Done_FreeType(mLibrary);
		mLibrary = NULL;
	}
}

int CVRenderText::setFont(const char* path_to_font)
{
	FT_Error error;
	mFontName = path_to_font;

	if (mFace) {
		FT_Done_Face(mFace);
		mFace = NULL;
	}

	if (!mLibrary) {		
		error = FT_Init_FreeType(&mLibrary);
		if (error != 0)
			return error;
	}

	return FT_New_Face(mLibrary, path_to_font, 0, &mFace);
}

int CVRenderText::renderText(cv::Mat &dstImg, cv::Point pos, const wchar_t* text, size_t textSize, Justify xMargin, Justify yMargin, 
							 cv::Scalar textColor, bool hasBackgrnd, cv::Scalar bgrColor, double bgrOpacity)
{
	FT_Error error;

	if (!mFace)
		return -1;

	error = FT_Set_Pixel_Sizes(mFace, textSize, textSize);
	if (error != 0)
		return error;

	// Get total width
	unsigned int total_width = 0;
	long max_top = 0;
	long min_bottom = 0;
	unsigned int max_height = 0;

	for (int i = 0; i < std::wcslen(text); i++) {
		FT_UInt glyph_index = FT_Get_Char_Index(mFace, text[i]);
		FT_BBox  bbox;
		FT_Glyph glyph;
		// Have to use FT_LOAD_RENDER.
		// If use FT_LOAD_DEFAULT, the actual glyph bitmap won't be loaded,
		// thus bitmap->rows will be incorrect, causing insufficient max_height.
		error = FT_Load_Glyph(mFace, glyph_index, FT_LOAD_RENDER);
		if (error != 0) {
			return error;
		}

		error = FT_Get_Glyph(mFace->glyph, &glyph);
		if (error != 0) {
			return error;
		}

		FT_Glyph_Get_CBox( glyph, FT_GLYPH_BBOX_PIXELS, &bbox );
		
		total_width += (mFace->glyph->advance.x >> 6);

		max_top = std::max(max_top, bbox.yMax + 1);
		min_bottom = std::min(min_bottom, bbox.yMin - 1);
		FT_Done_Glyph(glyph);
	}

	max_height = (unsigned int)(max_top - min_bottom);

	// Copy grayscale image from FreeType to OpenCV
	cv::Mat gray(max_height, total_width, CV_8UC1, cv::Scalar::all(0));
	int x = 0;
	for (int i = 0; i < std::wcslen(text); i++) {
		FT_UInt glyph_index = FT_Get_Char_Index(mFace, text[i]);
		FT_BBox  bbox;
		FT_Glyph glyph;

		error = FT_Load_Glyph(mFace, glyph_index, FT_LOAD_RENDER);
		if (error != 0) {
			return error;
		}

		error = FT_Get_Glyph(mFace->glyph, &glyph);
		if (error != 0) {
			return error;
		}

		FT_Glyph_Get_CBox( glyph, FT_GLYPH_BBOX_PIXELS, &bbox );

		FT_Bitmap *bitmap = &(mFace->glyph->bitmap);
		cv::Mat glyph_img(bitmap->rows, bitmap->width, CV_8UC1, bitmap->buffer, bitmap->pitch);
		int top = max_top - bbox.yMax;

		int left = x + mFace->glyph->bitmap_left;
		if (left < 0)
			left = 0;
		cv::Rect rect(left, top, bitmap->width, bitmap->rows);

		cv::Mat gray_part(gray, rect);
		glyph_img.copyTo(gray_part);

		x += (mFace->glyph->advance.x >> 6);
		FT_Done_Glyph(glyph);
	}

	// re-calculate position to render text over destination image
	switch (xMargin) {
	case CVRenderText::CENTER_MARGIN:
		pos.x -= total_width / 2;
		break;
	case CVRenderText::RIGHT_MARGIN:
		pos.x -= total_width;
		break;
	default:
		break;
	}

	switch (yMargin) {
	case CVRenderText::CENTER_MARGIN:
		pos.y -= max_height / 2;
		break;
	case CVRenderText::BOTTOM_MARGIN:
		pos.y -= max_height;
		break;
	default:
		break;
	}

	if (pos.x < 0)
		pos.x = 0;
	if (pos.y < 0)
		pos.y = 0;

	int width = total_width;
	int height = max_height;

	if (width > dstImg.cols)
		width = dstImg.cols;

	if (height > dstImg.rows)
		height = dstImg.rows;

	// get ROI actual from destination image
	cv::Rect rect(pos.x, pos.y, width, height);

	// rebuild ROI of image text overlayed in case of out of destination image
	cv::Rect rectText(0, 0, width, height);

	cv::Mat gray_bgr(gray.size(), CV_8UC1, cv::Scalar::all(255));
	cv::Mat gray_img(gray.size(), CV_8UC1, cv::Scalar::all(255));
	cv::Mat text_clr(gray.size(), CV_8UC3, textColor);
	cv::Mat bgrd_clr(gray.size(), CV_8UC3, bgrColor);
	cv::Mat coeffMat(gray.size(), CV_32FC3);
	std::vector<cv::Mat> coeff;

	// calculate gray for background
	cv::subtract(gray_bgr, gray, gray_bgr);
	// calculate gray for image
	cv::subtract(gray_img, gray, gray_img);

	// fill text color
	coeff.push_back(gray);
	coeff.push_back(gray);
	coeff.push_back(gray);

	cv::merge(coeff, coeffMat);
	cv::multiply(coeffMat, text_clr, text_clr, 1.0/255.0);

	if (hasBackgrnd) {
		// normalize opacity
		if (bgrOpacity > 1.0)
			bgrOpacity = 1.0;
		else if (bgrOpacity < 0.0)
			bgrOpacity = 0.0;

		double opacity = 0.9375*bgrOpacity + 0.0625; //(ax+b)
		gray_img *= (1.0 - opacity);
		gray_bgr *= opacity;

		// fill background color
		coeff.clear();
		coeff.push_back(gray_bgr);
		coeff.push_back(gray_bgr);
		coeff.push_back(gray_bgr);
		cv::merge(coeff, coeffMat);
		cv::multiply(coeffMat, bgrd_clr, bgrd_clr, 1.0/255.0);
	
		cv::Mat blendImg(dstImg, rect);
		cv::Mat blendText(text_clr, rectText);
		cv::Mat blendBgrd(bgrd_clr, rectText);

		// build overlayed ROI of destination image with gray scale
		coeff.clear();
		coeff.push_back(gray_img(rectText));
		coeff.push_back(gray_img(rectText));
		coeff.push_back(gray_img(rectText));
		cv::merge(coeff, coeffMat);
		cv::multiply(coeffMat, blendImg, blendImg, 1.0/255.0);

		// now merge text to image
		cv::add(blendImg, blendText, blendImg);
		cv::add(blendImg, blendBgrd, blendImg);
	}
	else {
		cv::Mat blendImg(dstImg, rect);
		cv::Mat blendText(text_clr, rectText);

		// build overlayed ROI of destination image with gray scale
		coeff.clear();
		coeff.push_back(gray_bgr(rectText));
		coeff.push_back(gray_bgr(rectText));
		coeff.push_back(gray_bgr(rectText));
		cv::merge(coeff, coeffMat);
		cv::multiply(coeffMat, blendImg, blendImg, 1.0/255.0);

		// now merge text to image
		cv::add(blendImg, blendText, blendImg);
	}

	

	return 0;
}