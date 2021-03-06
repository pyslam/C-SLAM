/*
 * c_vision,
 *
 *
 * Copyright (C) 2014 Davide Tateo
 * Versione 1.0
 *
 * This file is part of c_vision.
 *
 * c_vision is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * c_vision is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with c_vision.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "RecognizerLogic.h"

#include <sensor_msgs/image_encodings.h>
#include <opencv2/highgui/highgui.hpp>

#include <c_slam_msgs/NamedPolygon.h>

#include "ViewerManager.h"

using namespace ros;
using namespace sensor_msgs;
using namespace image_geometry;
using namespace cv_bridge;
using namespace cv;
using namespace std;

namespace enc = sensor_msgs::image_encodings;

RecognizerLogic::RecognizerLogic(NodeHandle n, ParameterServer& parameters) :
			BaseLogic(n, parameters), infoCache(15), imageCache(15),
			detector(parameters), dispP(parameters.getDisplayParams())
{
	cameraSubscriber = it.subscribeCamera(parameters.getCameraSource() + "/image_rect_color", 1,
				&RecognizerLogic::handleCamera, this);
	trackSubscriber = n.subscribe("tracks", 10, &RecognizerLogic::handleTrack,
				this);
	publisher = n.advertise<c_slam_msgs::NamedPolygon>("objects", 10);
}

void RecognizerLogic::handleCamera(const ImageConstPtr& msg,
			const CameraInfoConstPtr& info_msg)
{
	camera_frame_id = msg->header.frame_id;
	imageCache.add(msg);
	infoCache.add(info_msg);
}

void RecognizerLogic::handleTrack(const c_slam_msgs::TrackedObject& track)
{
	Mat objectImage, mask;
	Rect roi;
	CvImagePtr cv_ptr, cv_ptr_color;
	PinholeCameraModel cameraModel;

	try
	{
		ros::Time t = getImageData(track, cv_ptr, cv_ptr_color, cameraModel);
		getRoi(track, cv_ptr_color->image, roi, objectImage, mask);
		detect(objectImage, mask, track.id == dispP.currentObject);
		classify(cameraModel, roi, t, track.id);
		display(objectImage, track.id);
		detector.deleteDetections();
	}
	catch (cv_bridge::Exception& e)
	{
		ROS_ERROR("cv_bridge exception: %s", e.what());
	}
	catch (runtime_error& e)
	{
		ROS_WARN(e.what());
	}
}

void RecognizerLogic::detect(Mat& image, Mat& mask, bool showCanny)
{
	Mat greyFrame;

	detector.setRoll(roll);
	detector.detect(image, mask, showCanny);
}

void RecognizerLogic::classify(PinholeCameraModel& cameraModel, Rect& roi, ros::Time t, size_t id)
{
	c_fuzzy::Classification serviceCall;
	ObjectClassificator classificator(serviceCall, classifierParam);
	classificator.processFeatures(detector.getRectangles(), R);
	classificator.processFeatures(detector.getPoles(), R);
	classificator.processFeatures(detector.getClusters(), R);

	callClassificationService(serviceCall);

	classificator.labelFeatures();
	const vector<pair<vector<Point>, string> >& features =
				classificator.getGoodFeatures();

	sendFeatures(features, t, id);

}

void RecognizerLogic::display(Mat& image, unsigned int id)
{
	if(id != dispP.currentObject)
		return;

	ImageView& viewer = ViewerManager::getInstance().getView(id);
	viewer.setRectangles(detector.getRectangles());
	viewer.setPoles(detector.getPoles());
	viewer.setClusters(detector.getClusters());
	viewer.setRoll(roll);

	cv::Mat bigImage(360, 640, CV_8UC3, cv::Scalar(0,0,0));

	image.copyTo(bigImage(Rect(0, 0, image.cols, image.rows)));
	viewer.setImage(bigImage);

	viewer.display();

}

ros::Time RecognizerLogic::getImageData(const c_slam_msgs::TrackedObject& track,
			CvImagePtr& cv_ptr, CvImagePtr& cv_ptr_color,
			PinholeCameraModel& cameraModel)
{
	Time t = track.imageStamp + Duration(0, 100);
	const ImageConstPtr& img = imageCache.getElemBeforeTime(t);
	const CameraInfoConstPtr& info_msg = infoCache.getElemBeforeTime(t);


	if (img == NULL || info_msg == NULL)
		throw runtime_error("No valid image/info pairs in cache");

	cameraModel.fromCameraInfo(info_msg);

	cv_ptr = toCvCopy(img, enc::MONO8);
	cv_ptr_color = toCvCopy(img, enc::BGR8);

	return img->header.stamp;
}

void RecognizerLogic::getRoi(const c_slam_msgs::TrackedObject& track,
			Mat& input, Rect& roi, Mat& image, Mat& mask)
{
	vector<Point> polygon;

	roi.x = track.roi.x_offset;
	roi.y = track.roi.y_offset;
	roi.width = track.roi.width;
	roi.height = track.roi.height;

	for (int i = 0; i < track.polygon.points.size(); i++)
	{
		geometry_msgs::Point32 p = track.polygon.points[i];
		Point point(p.x - roi.x, p.y - roi.y);
		polygon.push_back(point);
	}

	//get the roi
	image = input(roi);

	//create the mask
	mask = Mat(image.size(), CV_8UC1);
	mask.setTo(0);
	fillConvexPoly(mask, polygon, 255, 8, 0);
}
