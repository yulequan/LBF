/*
Author: Bi Sai 
Date: 2014/06/18
This program is a reimplementation of algorithms in "Face Alignment by Explicit 
Shape Regression" by Cao et al.
If you find any bugs, please email me: soundsilencebisai-at-gmail-dot-com

Copyright (c) 2014 Bi Sai 
The MIT License (MIT)
Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include "FaceAlignment.h"
#include "LBFRegressor.h"
using namespace std;
using namespace cv;

Mat_<double> GetMeanShape(const vector<Mat_<double> >& shapes,
                          const vector<BoundingBox>& bounding_box){
    Mat_<double> result = Mat::zeros(shapes[0].rows,2,CV_64FC1);
    for(int i = 0;i < shapes.size();i++){
       
        result = result + ProjectShape(shapes[i],bounding_box[i]);
    }
    result = 1.0 / shapes.size() * result;
    return result;
}

void GetShapeResidual(const vector<Mat_<double> >& ground_truth_shapes,
                      const vector<Mat_<double> >& current_shapes,
                      const vector<BoundingBox>& bounding_boxs,
                      const Mat_<double>& mean_shape,
                      vector<Mat_<double> >& shape_residuals){
    
    Mat_<double> rotation;
    double scale;
    shape_residuals.resize(bounding_boxs.size());
    for (int i = 0;i < bounding_boxs.size(); i++){
        shape_residuals[i] = ProjectShape(ground_truth_shapes[i], bounding_boxs[i])
        - ProjectShape(current_shapes[i], bounding_boxs[i]);
        SimilarityTransform(mean_shape, ProjectShape(current_shapes[i],bounding_boxs[i]),rotation,scale);
        transpose(rotation,rotation);
        shape_residuals[i] = scale * shape_residuals[i] * rotation;
    }
}


Mat_<double> ProjectShape(const Mat_<double>& shape, const BoundingBox& bounding_box){
    Mat_<double> temp(shape.rows,2);
    for(int j = 0;j < shape.rows;j++){
        temp(j,0) = (shape(j,0)-bounding_box.centroid_x) / (bounding_box.width / 2.0);
        temp(j,1) = (shape(j,1)-bounding_box.centroid_y) / (bounding_box.height / 2.0);  
    } 
    return temp;  
}

Mat_<double> ReProjectShape(const Mat_<double>& shape, const BoundingBox& bounding_box){
    Mat_<double> temp(shape.rows,2);
    for(int j = 0;j < shape.rows;j++){
        temp(j,0) = (shape(j,0) * bounding_box.width / 2.0 + bounding_box.centroid_x);
        temp(j,1) = (shape(j,1) * bounding_box.height / 2.0 + bounding_box.centroid_y);
    } 
    return temp; 
}


void SimilarityTransform(const Mat_<double>& shape1, const Mat_<double>& shape2, 
                         Mat_<double>& rotation,double& scale){
    rotation = Mat::zeros(2,2,CV_64FC1);
    scale = 0;
    
    // center the data
    double center_x_1 = 0;
    double center_y_1 = 0;
    double center_x_2 = 0;
    double center_y_2 = 0;
    for(int i = 0;i < shape1.rows;i++){
        center_x_1 += shape1(i,0);
        center_y_1 += shape1(i,1);
        center_x_2 += shape2(i,0);
        center_y_2 += shape2(i,1); 
    }
    center_x_1 /= shape1.rows;
    center_y_1 /= shape1.rows;
    center_x_2 /= shape2.rows;
    center_y_2 /= shape2.rows;
    
    Mat_<double> temp1 = shape1.clone();
    Mat_<double> temp2 = shape2.clone();
    for(int i = 0;i < shape1.rows;i++){
        temp1(i,0) -= center_x_1;
        temp1(i,1) -= center_y_1;
        temp2(i,0) -= center_x_2;
        temp2(i,1) -= center_y_2;
    }

     
    Mat_<double> covariance1, covariance2;
    Mat_<double> mean1,mean2;
    // calculate covariance matrix
    calcCovarMatrix(temp1,covariance1,mean1,CV_COVAR_COLS);
    calcCovarMatrix(temp2,covariance2,mean2,CV_COVAR_COLS);

    double s1 = sqrt(norm(covariance1));
    double s2 = sqrt(norm(covariance2));
    scale = s1 / s2; 
    temp1 = 1.0 / s1 * temp1;
    temp2 = 1.0 / s2 * temp2;

    double num = 0;
    double den = 0;
    for(int i = 0;i < shape1.rows;i++){
        num = num + temp1(i,1) * temp2(i,0) - temp1(i,0) * temp2(i,1);
        den = den + temp1(i,0) * temp2(i,0) + temp1(i,1) * temp2(i,1);      
    }
    
    double norm = sqrt(num*num + den*den);    
    double sin_theta = num / norm;
    double cos_theta = den / norm;
    rotation(0,0) = cos_theta;
    rotation(0,1) = -sin_theta;
    rotation(1,0) = sin_theta;
    rotation(1,1) = cos_theta;
}

double calculate_covariance(const vector<double>& v_1, 
                            const vector<double>& v_2){
    Mat_<double> v1(v_1);
    Mat_<double> v2(v_2);
    double mean_1 = mean(v1)[0];
    double mean_2 = mean(v2)[0];
    v1 = v1 - mean_1;
    v2 = v2 - mean_2;
    return mean(v1.mul(v2))[0]; 
}
void LoadDate(string filepath,
              std::vector<cv::Mat_<uchar> >& images,
              std::vector<cv::Mat_<double> >& ground_truth_shapes,
              std::vector<BoundingBox> & bounding_box
              ){
    double img_num = 100;
    double landmark_num = 5;
    string image_name;
    cout << "Read images..." << endl;
    for(int i = 0;i < img_num;i++){
        image_name = filepath + to_string(i+1) + ".jpg";
        Mat_<uchar> temp = imread(image_name,0);
        images.push_back(temp);
    }
    
    ifstream fin;
    fin.open(filepath + "/boundingbox.txt");
    for(int i = 0;i < img_num;i++){
        BoundingBox temp;
        fin>>temp.start_x>>temp.start_y>>temp.width>>temp.height;
        temp.centroid_x = temp.start_x + temp.width/2.0;
        temp.centroid_y = temp.start_y + temp.height/2.0;
        bounding_box.push_back(temp);
    }
    fin.close();
    
    fin.open(filepath + "/keypoints.txt");
    for(int i = 0;i < img_num;i++){
        Mat_<double> temp(landmark_num,2);
        for(int j = 0;j < landmark_num;j++){
            fin>>temp(j,0);
        }
        for(int j = 0;j < landmark_num;j++){
            fin>>temp(j,1);
        }
        ground_truth_shapes.push_back(temp);
    }
    fin.close();
}