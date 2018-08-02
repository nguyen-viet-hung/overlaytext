#include "cvrendertext.h"
#include <cwchar>
#include <stdint.h>
#include <vector>

#include FT_GLYPH_H

#ifdef _MSC_VER
#pragma warning(disable:4996)
#endif

CVRenderText::CVRenderText()
	: mLibrary(NULL)
	, mStroker(NULL)
	, mFace(NULL)
	, mInitialized(false)
	, mFontName("") {
	FT_Error error;
	error = FT_Init_FreeType(&mLibrary);

	if (!error)
		mInitialized = true;
	else
		mInitialized = false;

	error = FT_Stroker_New(mLibrary, &mStroker);
	if (error || !mStroker) {
		mInitialized = false;
		FT_Done_FreeType(mLibrary);
		mLibrary = NULL;
	}

	mFace = NULL;
}

CVRenderText::~CVRenderText() {
	if (mFace) {
		FT_Done_Face(mFace);
		mFace = NULL;
	}

	if (mStroker) {
		FT_Stroker_Done(mStroker);
		mStroker = NULL;
	}

	if (mLibrary) {
		FT_Done_FreeType(mLibrary);
		mLibrary = NULL;
	}
}

int CVRenderText::setFont(const char* path_to_font) {
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

		error = FT_Stroker_New(mLibrary, &mStroker);
		if (error != 0 || !mStroker) {
			FT_Done_FreeType(mLibrary);
			mLibrary = NULL;
			return error;
		}
	}

	return FT_New_Face(mLibrary, path_to_font, 0, &mFace);
}

int CVRenderText::renderText(cv::Mat &dstImg, cv::Point pos, const wchar_t* text, size_t textSize, Justify xMargin, Justify yMargin, 
		cv::Scalar textColor, bool hasBorder, size_t brdSize, cv::Scalar brdColor, bool hasBackgrnd, cv::Scalar bgrColor, double bgrOpacity)
{
	FT_Error error;

	if (!mFace)
		return -1;

	error = FT_Set_Char_Size(mFace, 0, textSize * 64, 0, 0);
	if (error != 0)
		return error;

	if (hasBorder && mStroker)
		FT_Stroker_Set(mStroker, brdSize * 64, FT_STROKER_LINECAP_ROUND, FT_STROKER_LINEJOIN_ROUND, 0);

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
		error = FT_Load_Glyph(mFace, glyph_index, FT_LOAD_DEFAULT);
		if (error != 0) {
			return error;
		}

		error = FT_Get_Glyph(mFace->glyph, &glyph);
		if (error != 0) {
			return error;
		}

		if (hasBorder) {
			FT_Glyph_StrokeBorder(&glyph, mStroker, false, true);
		}

		FT_Glyph_To_Bitmap(&glyph, FT_RENDER_MODE_NORMAL, nullptr, true);
		FT_Glyph_Get_CBox( glyph, FT_GLYPH_BBOX_TRUNCATE, &bbox );
		FT_BitmapGlyph bitmapGlyph = reinterpret_cast<FT_BitmapGlyph>(glyph);
		
		total_width += std::max(bbox.xMax, mFace->glyph->advance.x >> 6);
		if (hasBorder)
			total_width += brdSize;

		max_top = std::max(max_top, bbox.yMax + 1);
		min_bottom = std::min(min_bottom, bbox.yMin - 1);
		FT_Done_Glyph(glyph);
	}

	max_height = (unsigned int)(max_top - min_bottom);

	// Copy grayscale image from FreeType to OpenCV
	cv::Mat gray_outline(max_height, total_width, CV_8UC1, cv::Scalar::all(0));
	cv::Mat gray_text(max_height, total_width, CV_8UC1, cv::Scalar::all(0));
	int x = 0;
	for (int i = 0; i < std::wcslen(text); i++) {
		FT_UInt glyph_index = FT_Get_Char_Index(mFace, text[i]);
		FT_BBox  bbox;
		FT_Glyph glyph;

		error = FT_Load_Glyph(mFace, glyph_index, FT_LOAD_DEFAULT);
		if (error != 0) {
			return error;
		}

		error = FT_Get_Glyph(mFace->glyph, &glyph);
		if (error != 0) {
			return error;
		}

		if (hasBorder) {
			// create outline
			FT_Glyph_StrokeBorder(&glyph, mStroker, false, true);
			FT_Glyph_To_Bitmap(&glyph, FT_RENDER_MODE_NORMAL, nullptr, true);
			FT_Glyph_Get_CBox( glyph, FT_GLYPH_BBOX_TRUNCATE, &bbox );

			FT_BitmapGlyph bitmapGlyph = reinterpret_cast<FT_BitmapGlyph>(glyph);
			cv::Mat glyph_img(bitmapGlyph->bitmap.rows, bitmapGlyph->bitmap.width, CV_8UC1, bitmapGlyph->bitmap.buffer, bitmapGlyph->bitmap.pitch);
			int top = max_top - bbox.yMax;

			int left = x + bitmapGlyph->left;
			if (left < 0)
				left = 0;
			cv::Rect rect(left, top, bitmapGlyph->bitmap.width, bitmapGlyph->bitmap.rows);

			cv::Mat gray_part(gray_outline, rect);
			glyph_img.copyTo(gray_part);

			int max_x = std::max(bbox.xMax, mFace->glyph->advance.x >> 6);
			
			FT_Done_Glyph(glyph);
			//create text
			error = FT_Get_Glyph(mFace->glyph, &glyph);
			if (error != 0) {
				return error;
			}

			FT_Glyph_To_Bitmap(&glyph, FT_RENDER_MODE_NORMAL, nullptr, true);
			FT_Glyph_Get_CBox( glyph, FT_GLYPH_BBOX_TRUNCATE, &bbox );

			bitmapGlyph = reinterpret_cast<FT_BitmapGlyph>(glyph);
			glyph_img = cv::Mat(bitmapGlyph->bitmap.rows, bitmapGlyph->bitmap.width, CV_8UC1, bitmapGlyph->bitmap.buffer, bitmapGlyph->bitmap.pitch);
			top = max_top - bbox.yMax;

			left = x + bitmapGlyph->left;
			if (left < 0)
				left = 0;
			rect = cv::Rect(left, top, bitmapGlyph->bitmap.width, bitmapGlyph->bitmap.rows);

			gray_part = cv::Mat(gray_text, rect);
			glyph_img.copyTo(gray_part);

			x += max_x;
			if (hasBorder)
				x += brdSize;
			FT_Done_Glyph(glyph);
		} else {
			FT_Glyph_To_Bitmap(&glyph, FT_RENDER_MODE_NORMAL, nullptr, true);
			FT_Glyph_Get_CBox( glyph, FT_GLYPH_BBOX_PIXELS, &bbox );

			FT_BitmapGlyph bitmapGlyph = reinterpret_cast<FT_BitmapGlyph>(glyph);
			cv::Mat glyph_img(bitmapGlyph->bitmap.rows, bitmapGlyph->bitmap.width, CV_8UC1, bitmapGlyph->bitmap.buffer, bitmapGlyph->bitmap.pitch);
			int top = max_top - bbox.yMax;

			int left = x + bitmapGlyph->left;
			if (left < 0)
				left = 0;
			cv::Rect rect(left, top, bitmapGlyph->bitmap.width, bitmapGlyph->bitmap.rows);

			cv::Mat gray_part(gray_outline, rect);
			glyph_img.copyTo(gray_part);

			x += std::max(bbox.xMax, mFace->glyph->advance.x >> 6);
			FT_Done_Glyph(glyph);
		}
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

	if (width > dstImg.cols - pos.x)
		width = dstImg.cols - pos.x;

	if (height > dstImg.rows - pos.y)
		height = dstImg.rows - pos.y;

	// get ROI actual from destination image
	cv::Rect rect(pos.x, pos.y, width, height);

	// rebuild ROI of image text overlayed in case of out of destination image
	cv::Rect rectText(0, 0, width, height);

	cv::Mat gray_bgr(gray_outline.size(), CV_8UC1, cv::Scalar::all(255));
	cv::Mat gray_img(gray_outline.size(), CV_8UC1, cv::Scalar::all(255));
	cv::Mat outline_clr(gray_outline.size(), CV_8UC3, brdColor);
	cv::Mat text_clr(gray_text.size(), CV_8UC3, textColor);
	cv::Mat bgrd_clr(gray_outline.size(), CV_8UC3, bgrColor);
	cv::Mat coeffMat(gray_outline.size(), CV_32FC3);
	std::vector<cv::Mat> coeff;

	// calculate gray for background
	cv::subtract(gray_bgr, gray_outline, gray_bgr);
	// calculate gray for image
	cv::subtract(gray_img, gray_outline, gray_img);
	
	if (hasBorder) {
		// fill outline color
		coeff.push_back(gray_outline);
		coeff.push_back(gray_outline);
		coeff.push_back(gray_outline);

		cv::merge(coeff, coeffMat);
		cv::multiply(coeffMat, outline_clr, outline_clr, 1.0/255.0);
		// fill text color
		coeff.clear();
		coeff.push_back(gray_text);
		coeff.push_back(gray_text);
		coeff.push_back(gray_text);
		cv::merge(coeff, coeffMat);
		cv::multiply(coeffMat, text_clr, text_clr, 1.0/255.0);
	} else {
		// fill text color
		coeff.push_back(gray_outline);
		coeff.push_back(gray_outline);
		coeff.push_back(gray_outline);

		cv::merge(coeff, coeffMat);
		cv::multiply(coeffMat, text_clr, text_clr, 1.0/255.0);
	}

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
		cv::Mat blendOutline(outline_clr, rectText);
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
		if (hasBorder) {
			cv::add(blendImg, blendOutline, blendImg);

			gray_img = cv::Mat(gray_text.size(), CV_8UC1, cv::Scalar::all(255));
			cv::subtract(gray_img, gray_text, gray_img);
			coeff.clear();
			coeff.push_back(gray_img(rectText));
			coeff.push_back(gray_img(rectText));
			coeff.push_back(gray_img(rectText));
			cv::merge(coeff, coeffMat);
			cv::multiply(coeffMat, blendImg, blendImg, 1.0/255.0);
			cv::add(blendImg, blendText, blendImg);
		} else {
			cv::add(blendImg, blendText, blendImg);
		}

		cv::add(blendImg, blendBgrd, blendImg);
	}
	else {
		cv::Mat blendImg(dstImg, rect);
		cv::Mat blendOutline(outline_clr, rectText);
		cv::Mat blendText(text_clr, rectText);

		// build overlayed ROI of destination image with gray scale
		coeff.clear();
		coeff.push_back(gray_bgr(rectText));
		coeff.push_back(gray_bgr(rectText));
		coeff.push_back(gray_bgr(rectText));
		cv::merge(coeff, coeffMat);
		cv::multiply(coeffMat, blendImg, blendImg, 1.0/255.0);

		// now merge text to image
		if (hasBorder) {
			cv::add(blendImg, blendOutline, blendImg);
			gray_img = cv::Mat(gray_text.size(), CV_8UC1, cv::Scalar::all(255));
			cv::subtract(gray_img, gray_text, gray_img);
			coeff.clear();
			coeff.push_back(gray_img(rectText));
			coeff.push_back(gray_img(rectText));
			coeff.push_back(gray_img(rectText));
			cv::merge(coeff, coeffMat);
			cv::multiply(coeffMat, blendImg, blendImg, 1.0/255.0);
			cv::add(blendImg, blendText, blendImg);
		} else {
			cv::add(blendImg, blendText, blendImg);
		}
	}

	

	return 0;
}

int CVRenderText::renderText(cv::Mat &dstImg, cv::Point pos, const char* text, size_t textSize, Justify xMargin, Justify yMargin, 
		cv::Scalar textColor, bool hasBorder, size_t brdSize, cv::Scalar brdColor, bool hasBackgrnd, cv::Scalar bgrColor, double bgrOpacity)
{
	std::string mb(text);
	std::wstring ws(mb.size(), L' ');

	ws.resize(std::mbstowcs(&ws[0], mb.c_str(), mb.size()));
	return renderText(dstImg, pos, ws.c_str(), textSize, xMargin, yMargin, textColor, hasBorder, brdSize, brdColor, hasBackgrnd, bgrColor, bgrOpacity);
}