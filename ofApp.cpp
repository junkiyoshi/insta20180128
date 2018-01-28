#include "ofApp.h"

//--------------------------------------------------------------
void ofApp::setup() {
	ofSetFrameRate(30);
	ofBackground(0);
	ofSetWindowTitle("InstaKinect");

	ofEnableDepthTest();

	// Create Instance
	this->sensor = nullptr;
	HRESULT result = S_OK;
	result = NuiCreateSensorByIndex(0, &sensor);
	if (FAILED(result)) {
		cout << "Error : Create Sensor" << endl;
		return;
	}

	// Initialize
	result = sensor->NuiInitialize(NUI_INITIALIZE_FLAG_USES_COLOR | NUI_INITIALIZE_FLAG_USES_DEPTH);
	if (FAILED(result)) {
		cout << "Error : Initialise COLOR" << endl;
		return;
	}

	// Color
	this->color_event = INVALID_HANDLE_VALUE;
	this->color_handle = INVALID_HANDLE_VALUE;
	color_event = CreateEventW(nullptr, true, false, nullptr);
	result = sensor->NuiImageStreamOpen(NUI_IMAGE_TYPE_COLOR, NUI_IMAGE_RESOLUTION_640x480, 0, 2, color_event, &color_handle);
	if (FAILED(result)) {
		cout << "Error : Color Stream Open" << endl;
		return;
	}

	// Depth
	this->depth_event = INVALID_HANDLE_VALUE;
	this->depth_handle = INVALID_HANDLE_VALUE;
	depth_event = CreateEventW(nullptr, true, false, nullptr);
	result = sensor->NuiImageStreamOpen(NUI_IMAGE_TYPE_DEPTH, NUI_IMAGE_RESOLUTION_640x480, 0, 2, depth_event, &depth_handle);
	if (FAILED(result)) {
		cout << "Error : Depth Stream Open" << endl;
		return;
	}

	// Size
	unsigned long refWidth = 0;
	unsigned long refHeight = 0;
	NuiImageResolutionToSize(NUI_IMAGE_RESOLUTION_640x480, refWidth, refHeight);
	this->width = static_cast<int>(refWidth);
	this->height = static_cast<int>(refHeight);

	// Mapp
	this->cordinate_mapper = nullptr;
	result = sensor->NuiGetCoordinateMapper(&this->cordinate_mapper);
	if (FAILED(result)) {
		cout << "Error : Cordinate Mapper " << endl;
		return;
	}

	// OpenCV
	this->frame_img.allocate(this->width, this->height, OF_IMAGE_COLOR);
	this->frame = cv::Mat(this->frame_img.getHeight(), this->frame_img.getWidth(), CV_MAKETYPE(CV_8UC3, this->frame_img.getPixels().getNumChannels()), this->frame_img.getPixels().getData(), 0);
	this->depth = cv::Mat(height, width, CV_16UC1);

	this->events[0] = this->color_event;
	this->events[1] = this->depth_event;

	this->font.loadFont("fonts/Kazesawa-Bold.ttf", 7);
}

//--------------------------------------------------------------
void ofApp::update() {

	HRESULT result = S_OK;

	// Color Table
	cv::Vec3b color[7];
	color[0] = cv::Vec3b(0, 0, 0);
	color[1] = cv::Vec3b(255, 0, 0);
	color[2] = cv::Vec3b(0, 255, 0);
	color[3] = cv::Vec3b(0, 0, 255);
	color[4] = cv::Vec3b(255, 255, 0);
	color[5] = cv::Vec3b(255, 0, 255);
	color[6] = cv::Vec3b(0, 255, 255);

	std::vector<NUI_COLOR_IMAGE_POINT> color_point(width * height);

	// Wait frame update.
	ResetEvent(this->color_event);
	ResetEvent(this->depth_event);
	WaitForMultipleObjects(ARRAYSIZE(this->events), this->events, true, INFINITE);

	// Get frame from color camera.
	NUI_IMAGE_FRAME colorImageFrame = { 0 };
	result = sensor->NuiImageStreamGetNextFrame(this->color_handle, 0, &colorImageFrame);
	if (FAILED(result)) {
		std::cerr << "Error : NuiImageStreamGetNextFrame( COLOR )" << std::endl;
		return;
	}

	// Get image data.
	INuiFrameTexture* pColorFrameTexture = colorImageFrame.pFrameTexture;
	NUI_LOCKED_RECT colorLockedRect;
	pColorFrameTexture->LockRect(0, &colorLockedRect, nullptr, 0);

	//// Get frame from depth camera.
	NUI_IMAGE_FRAME depthImageFrame = { 0 };
	result = this->sensor->NuiImageStreamGetNextFrame(this->depth_handle, 0, &depthImageFrame);
	if (FAILED(result)) {
		std::cerr << "Error : NuiImageStreamGetNextFrame( DEPTH )" << std::endl;
		return;
	}

	// Get depth data.
	BOOL nearMode = false;
	INuiFrameTexture* pDepthFrameTexture = nullptr;
	this->sensor->NuiImageFrameGetDepthImagePixelFrameTexture(this->depth_handle, &depthImageFrame, &nearMode, &pDepthFrameTexture);
	NUI_LOCKED_RECT depthLockedRect;
	pDepthFrameTexture->LockRect(0, &depthLockedRect, nullptr, 0);

	// OpenCV(image)
	cv::Mat src = cv::Mat(this->height, this->width, CV_8UC4, reinterpret_cast<unsigned char*>(colorLockedRect.pBits));
	cv::cvtColor(src, this->frame, CV_BGRA2RGB);
	cv::imshow("frame", this->frame);

	// OpenCV(depth)
	cv::Mat bufferMat = cv::Mat::zeros(height, width, CV_16UC1);
	NUI_DEPTH_IMAGE_PIXEL* depth_pixel = reinterpret_cast<NUI_DEPTH_IMAGE_PIXEL*>(depthLockedRect.pBits);
	this->cordinate_mapper->MapDepthFrameToColorFrame(NUI_IMAGE_RESOLUTION_640x480, width * height, depth_pixel, NUI_IMAGE_TYPE_COLOR, NUI_IMAGE_RESOLUTION_640x480, width * height, &color_point[0]);
	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			unsigned int index = y * width + x;
			bufferMat.at<unsigned short>(color_point[index].y, color_point[index].x) = depth_pixel[index].depth;
		}
	}
	bufferMat.copyTo(this->depth);
	//cv::imshow("depth", this->depth);

	// Release
	pColorFrameTexture->UnlockRect(0);
	pDepthFrameTexture->UnlockRect(0);
	this->sensor->NuiImageStreamReleaseFrame(this->color_handle, &colorImageFrame);
	this->sensor->NuiImageStreamReleaseFrame(this->depth_handle, &depthImageFrame);

	this->light.setPosition(ofVec3f(0, 0, 4096));
	ofEnableLighting();
	this->light.enable();
}

//--------------------------------------------------------------
void ofApp::draw() {

	this->cam.begin();
	ofTranslate(-ofGetWidth() / 2, -ofGetHeight() / 2, -512);
	ofRotateX(180);

	for (int y = 0; y < this->depth.rows; y += 5) {

		for (int x = 0; x < this->depth.cols; x += 5) {

			int z = depth.at<unsigned short>(y, x);

			if (z != 0 && z < 1024 + 512) {
				ofDrawBox(ofVec3f(x, y, z), 8);
			}
			else {

			}
		}
	}

	this->cam.end();
}

//========================================================================
int main() {
	ofSetupOpenGL(720, 720, OF_WINDOW);
	ofRunApp(new ofApp());
}