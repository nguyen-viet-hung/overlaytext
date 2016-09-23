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

	renderer.setFont("./times.ttf");
	renderer.renderText(img, cv::Point(img.rows / 2, img.cols / 2), L"Việt Nam sample text", 50, CVRenderText::CENTER_MARGIN, CVRenderText::CENTER_MARGIN, 
		cv::Scalar(0, 0, 255), cv::Scalar(128, 128, 0), 0.4);

	cv::imwrite("./result.jpg", img);

	return 0;
}