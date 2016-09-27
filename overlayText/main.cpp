#include <ft2build.h>
#include FT_FREETYPE_H

#include <iostream>
#include <conio.h>
#include "cvrendertext.h"
#include <opencv2/highgui/highgui.hpp>


int main (int argc, char** argv)
{
	CVRenderText renderer;
	cv::Mat img = cv::imread("./input.jpg");

	renderer.setFont("./malgun.ttf");

	renderer.renderText(img, cv::Point(0, 0), L"희나리", 100, CVRenderText::LEFT_MARGIN, CVRenderText::TOP_MARGIN, 
		cv::Scalar(255, 255, 255), true, cv::Scalar(0, 0, 0), 0.1);

	renderer.setFont("./times.ttf");
	renderer.renderText(img, cv::Point(img.cols / 2, img.rows / 2), L"Việt Nam sample text", 50, CVRenderText::CENTER_MARGIN, CVRenderText::CENTER_MARGIN, 
		cv::Scalar(0, 0, 255), true, cv::Scalar(128, 128, 0), 0.4);

	renderer.renderText(img, cv::Point(0, img.rows), L"another Việt Nam sample text", 70, CVRenderText::LEFT_MARGIN, CVRenderText::BOTTOM_MARGIN, 
		cv::Scalar(0, 255, 255), false);

	cv::imwrite("./result.jpg", img);

	return 0;
}