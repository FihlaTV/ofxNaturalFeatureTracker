/*
 * ImageTrackerLib.h
 *
 *  Created on: Feb 17, 2015
 *      Author: roy_shilkrot
 *
 *  The MIT License (MIT)
 *
 *  Copyright (c) 2015 Roy Shilkrot and Valentin Heun
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in
 *  all copies or substantial portions of the Software.
 
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 *  THE SOFTWARE. *
 */

#ifndef IMAGETRACKERLIB_H_
#define IMAGETRACKERLIB_H_

#include <opencv2/opencv.hpp>
#include <opencv2/nonfree/features2d.hpp>
#include <vector>
#include <ofThread.h>
#include <ofVideoGrabber.h>

namespace ImageTrackerLib {

using namespace cv;
using namespace std;

/* Tracker
 * A basic natural features tracker for AR, that is given a trackable image to track and
 * then will detect and track it in a video stream. 
 * 
 * It maintains its own runloop so not to disturb other processing threads, and provides a model-view matrix
 * in OpenGL standars for any 3D augmentation.
 * 
 * It will switch automatically from bootstrap to optical-flow tracking based on the number of detected
 * features, and will try to keep itself lean and fast while maintaining a strong 3D pose estimation.
 */
class Tracker : public ofThread  {
private:
    Ptr<FeatureDetector>        detector;
    Ptr<DescriptorExtractor>    extractor;
    Ptr<DescriptorMatcher>      matcher;
    Mat                         marker_frame, marker_desc;
    vector<KeyPoint>            marker_kp;
    vector<Point2f>             obj_bb;
    bool                        bootstrap;
    Mat_<float>                 camMat;

    vector<KeyPoint>            trackedFeatures;
    vector<int>                 trackedFeaturesOnMarker;
    Mat                         prevGray;
    Mat                         toProcessFrame;
    Mat                         raux,taux;
    Mat                         homography;
    Mat                         cvToGl;
    
    bool                        tracking;
    bool                        debug;
    bool                        newFrame;
    
    virtual void                threadedFunction();
public:
    Mat                         outputFrame;
    Mat                         hmask;
    Mat_<float>                 modelViewMatrix;

    Tracker(Mat_<float> cam, Ptr<FeatureDetector>, Ptr<DescriptorExtractor>);
    void update();
    int setMarker(const Mat& marker);
    Mat getMarkerMask();
    void bootstrapTracking(const Mat& frame, const Mat& useHomography = Mat(), const Mat& mask = Mat());
    void track(const Mat& frame);
    Mat process(const Mat& frame, const Mat& mask = Mat());
    void calcModelViewMatrix(Mat_<float>& modelview_matrix, Mat_<float>& camMat);

    const Ptr<Feature2D>& getDetector() const { return detector; }
    const vector<KeyPoint>& getTrackedFeatures() const { return trackedFeatures; }
    bool isTracking() const { return tracking || bootstrap; }
    bool canCalcModelViewMatrix() const;
    void setDebug(bool b) { debug = b; }
    void setToProcessFrame(const Mat& f) { lock(); toProcessFrame = f; unlock(); newFrame = true; }
    Mat_<float> getModelViewMatrix() {
        if(modelViewMatrix.empty()) return Mat_<float>::eye(4,4);
        Mat_<float> tmp; lock(); modelViewMatrix.copyTo(tmp); unlock(); return tmp; }

    void reset() {
        trackedFeatures.clear();
        trackedFeaturesOnMarker.clear();
        tracking = true;
        bootstrap = true;
        raux.release();
        taux.release();
    }
};
    
/* MarkerDetector
 * A Bag-of-Visual-Words detector that can be trained to detect markers in a scene.
 * Can also save and load it's state from the filesystem
 */
class MarkerDetector {
    Ptr<BOWKMeansTrainer>            bowtrainer;
    Ptr<BOWImgDescriptorExtractor>   bowextractor;
    Ptr<DescriptorMatcher>           matcher;
    Ptr<FeatureDetector>             detector;
    Ptr<DescriptorExtractor>         extractor;
    Mat                              vocabulary;
    vector<Mat>                      markers;
    vector<string>                   marker_files;
    PCA                              descriptorPCA;
    Mat                              descriptorsBeforePCA;
    Mat                              descriptorsAfterPCA;
    CvKNearest                       classifier;
    Mat                              training;
    vector<string>                   training_labels;
    vector<string>                   training_labelsUniq;

public:
    MarkerDetector();
    void readFromFiles();
    void saveToFiles();
    void addMarker(const string& marker_file);
    void addMarker(const Mat& marker, const string& marker_file);
    void cluster();
    void extractBOWdescriptor(const Mat& img, Mat& imgDescriptor, const Mat& mask = Mat());
    void addImageToTraining(const Mat& img,const string& label);
    string detectMarkerInImage(const Mat& img, const Mat& mask = Mat());
    Mat getMarker(const string& label);
    void setVocabulary(const Mat& vocabulary);

    const Mat& getVocabulary() const {return vocabulary;}
    const Mat& getTraining() const { return training; }
    void setTraining(const Mat& training) { this->training = training;  }
    vector<string> getTrainingLabels() const { return training_labels;  }
    void setTrainingLabels(vector<string> trainingLabels) { training_labels = trainingLabels; }
    const PCA& getDescriptorPca() const {  return descriptorPCA; }
    void setDescriptorPca(const PCA& descriptorPca) { descriptorPCA = descriptorPca;  }
    vector<string> getMarkerFiles() const {  return marker_files; }
    void setMarkerFiles(vector<string> markerFiles) {  marker_files = markerFiles; }
};
   
/* SimpleAdHocTracker
 * A tracker that creates an ad-hoc marker from any trackable surface by using
 * structure-from-motion (stereo triangulation) as a bootstrapping step
 */
class SimpleAdHocTracker {
    Ptr<FeatureDetector>    detector;
    bool                    bootstrapping;
    vector<KeyPoint>        bootstrap_kp;
    vector<KeyPoint>        trackedFeatures;
    vector<Point3d>         trackedFeatures3D;
    Mat                     prevGray;
    Mat                     camMat;
    bool                    canCalcMVM;
    Mat                     raux,taux;
    Mat                     cvToGl;
    Mat_<double>            modelview_matrix;
    
public:
    SimpleAdHocTracker(const Ptr<FeatureDetector>&, const Mat& cam);
    void bootstrap(const Mat&);
    void bootstrapTrack(const Mat&);
    void track(const Mat&);
    void process(const Mat&, bool newmap = false);
    bool canCalcModelViewMatrix() const;
    void calcModelViewMatrix(Mat_<double>& modelview_matrix);
    bool triangulateAndCheckReproj(const Mat& P, const Mat& P1);
    bool cameraPoseAndTriangulationFromFundamental();
    bool DecomposeEtoRandT(Mat_<double>& E, Mat_<double>& R1, Mat_<double>& R2, Mat_<double>& t1, Mat_<double>& t2);
    
    const vector<KeyPoint>& getTrackedFeatures() const {
        return trackedFeatures;
    }
    const vector<Point3d>& getTracked3DFeatures() const {
        return trackedFeatures3D;
    }
};

/* ImageTracker
 * A class to manage the individual marker trackers and the marker detector.
 */
class ImageTracker : public ofThread {
    std::vector<ofPtr<Tracker> >    trackers;
    MarkerDetector                  markerDetector;
    ofVideoGrabber*                 grabber;
    Mat                             toProcessFrame;
    bool                            debug;
    Mat_<float>                     camMat;
    ofTexture                       tex;
    Ptr<FeatureDetector>            detector;
    Ptr<DescriptorExtractor>        extractor;
    
    virtual void                    threadedFunction();
public:
    Mat_<float>                     persp;

    ImageTracker(ofVideoGrabber* g);
    virtual ~ImageTracker();
    
    void setup();
    void update();
    void setDebug(bool b) { debug = b; }

    void draw(int w, int h) {
        tex.loadData(toProcessFrame.data, toProcessFrame.cols, toProcessFrame.rows, GL_RGB);
        tex.draw(0, 0, w, h);
    }

    const vector<ofPtr<Tracker> >& getTrackers() const { return trackers; }
};

} /* namespace ImageTrackerLib */

#endif /* IMAGETRACKERLIB_H_ */
