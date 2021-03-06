#include "../JuceLibraryCode/JuceHeader.h"
using namespace RAPT;
using namespace rosic;

//=================================================================================================

/** A class to let console applications show their progress when performing a long task. It 
repeatedly writes a "percentage done" in the format " 45%" to the console. Note that the initial
whitespace is intentional to allow for the first digit when it hits 100%. It deletes and overwrites
the old percentage by programmatically writing backspaces to the console, so the indicator doesn't
fill up the console - it just stays put and counts up. */

class rsConsoleProgressIndicator
{

public:

  // todo: have a setting: setNumDecimalPlaces

  void deleteCharactersFromConsole(int numChars) const
  {    
    unsigned char back = 8;                // backspace, see http://www.asciitable.com/
    for(int i = 1; i <= numChars; i++)
      std::cout << back;                   // delete  characters
    // can we do better? like first creating a string of backspaces and writing it to cout in one 
    // go?
  }

  std::string getPercentageString(double precentage) const 
  {
    int percent = (int) round(precentage);
    // pre-padding (this is dirty - can we do this more elegantly?)
    std::string s;
    if(percent < 100) s += " ";
    if(percent < 10)  s += " ";
    s += std::to_string(percent) + "%";
    return s;
  }
  // always prodcues a 4 character string with a rounded percentage value
  // todo: produce a 7 character string with two decimal places after the dot, like:
  // " 56.87%" (the leading whitespace is intentional)
  // for longer tasks, we may need a finer indicator 

  /** Prints the initial 0% in the desired format (i.e. with padding) to the console. */
  void init() const
  {
    std::cout << getPercentageString(0.0);
  }

  /** Deletes the old percentage from the console and prints the new one. */
  void update(int itemsDone, int lastItemIndex) const
  {
    deleteCharactersFromConsole(4);                  // delete old percentage from console
    double percentDone = 100.0 * double(itemsDone) / double(lastItemIndex);
    std::cout << getPercentageString(percentDone);   // write the new percentage to console
  }
  // maybe implement a progress bar using ascii code 178 or 219
  // http://www.asciitable.com/
  // maybe make the function mor general: itemsDone, lastItemIndex

  // have a function init that write 0% in the desired format


protected:

  //int numDecimals = 0; // 0: only the rounded integer percentage is shown

};

//=================================================================================================

/** A class for representing videos. It is mostly intended to be used to accumulate frames of an
animation into an array of images. */

class rsVideoRGB
{

public:

  rsVideoRGB(int width, int height/*, int frameRate*/)
  {
    this->width     = width;
    this->height    = height;
    //this->frameRate = frameRate;
  }


  //-----------------------------------------------------------------------------------------------
  /** \name Setup */

  /** Selects, whether or not the pixel values should be clipped to the valid range. If they are
  not clipped, they will wrap around to zero when they overflow the valid range. */
  void setPixelClipping(bool shouldClip) { clipPixelValues = shouldClip; }


  //-----------------------------------------------------------------------------------------------
  /** \name Inquiry */

  /** Returns the pixel width. */
  int getWidth() const { return width; }

  /** Returns the pixle height. */
  int getHeight() const { return height; }

  /** Returns the number of frames. */
  int getNumFrames() const { return (int) frames.size(); }

  /** Returns a const-reference to the i-th frame. */
  const rsImage<rsPixelRGB>& getFrame(int i) const { return frames[i]; }


  //-----------------------------------------------------------------------------------------------
  /** \name Manipulations */

  /** Appends a frame to the video. The images for the r,g,b channels must have the right width
  and height. */
  void appendFrame(const rsImage<float>& R, const rsImage<float>& G, const rsImage<float>& B)
  {
    rsAssert(R.hasShape(width, height));
    rsAssert(G.hasShape(width, height));
    rsAssert(B.hasShape(width, height));
    frames.push_back(rsConvertImage(R, G, B, clipPixelValues));
  }

  // have a function that just accepts pointers to float for r,g,b along with width,height, so we
  // can also use it with rsMatrix, etc. - it shouldn't depend on frames being represented as 
  // rsImage - maybe also allow for rsMultiArray (interpret a 3D array as video) 
  // -> appendFrames(rsMultiArray<float>& R
  // the range of the 1st index is the number of frames to be appended
  

protected:

  // video parameters:
  int width  = 0;
  int height = 0;
  //int frameRate = 0;  // can be decided when writing the file

  // audio parameters:
  // int numChannels = 0;
  // int sampleRate  = 0;

  // conversion parameters:
  bool clipPixelValues = false;  // clip to valid range (otherwise wrap-around)

  // video data:
  std::vector<rsImage<rsPixelRGB>> frames;

  // audio data:
  // todo: have support for (multichannel) audio
  // float** audioSamples = nullptr;
};

//=================================================================================================

/** A class for writing videos (of type rsVideoRGB) to files. It will first write all the frames as
separate temporary .ppm files to disk and then invoke ffmpeg to combine them into an mp4 video 
file. It requires ffmpeg to be available. On windows, this means that ffmpeg.exe must either be 
present in the current working directory (typically the project directory) or it must be installed 
somewhere on the system and the installation path must be one of the paths given the "PATH" 
environment variable. 

todo: maybe do it like in GNUPlotCpp: use an ffmpegPath member variable
*/

class rsVideoFileWriter
{

public:

  //-----------------------------------------------------------------------------------------------
  /** \name Setup */

  void setFrameRate(int newRate) { frameRate = newRate; }

  /** Sets the amount of compression where 0 means lossless and 51 gives the highest compression
  and worst quality. Corresponds to the CRF (constant rate factor) encoder setting, see:  
  https://trac.ffmpeg.org/wiki/Encode/H.264  */
  void setCompressionLevel(int newLevel) { compression = newLevel; }

  /** Selects, whether or not the temporary image files should be deleted after the final video has 
  been created. By default, they are deleted but keeping them can be useful for two reasons: 
  1: they are full quality, 2: it is possible to use the left/right keys in IrfanView to manually 
  "play" the video forward or backward - just load one of the ppm files and use left/right. */
  void setDeleteTemporaryFiles(bool shouldDelete) { cleanUp = shouldDelete; }

  // todo: setOutputFileName, setOutputDirectory, setTempDirectory


  //-----------------------------------------------------------------------------------------------
  /** \name File writing */

  /** Writes the given video into a file with given name. The name should NOT include the .mp4 
  extension - this will be added here. */
  void writeVideoToFile(const rsVideoRGB& vid, const std::string& fileName) const
  {
    writeTempFiles(vid);
    encodeTempFiles(fileName);
    if(cleanUp)
      deleteTempFiles(vid.getNumFrames());
  }

  /** Writes the temporary .ppm files to the harddisk for the given video. */
  void writeTempFiles(const rsVideoRGB& vid) const
  {
    int numFrames = vid.getNumFrames();
    std::cout << "Writing ppm files: ";
    progressIndicator.init();
    for(int i = 0; i < numFrames; i++) {
      std::string path = getTempFileName(i);
      writeImageToFilePPM(vid.getFrame(i), path.c_str());
      progressIndicator.update(i, numFrames-1);
    }
    std::cout << "\n\n";
  }

  /** Combines the temporary .ppm files that have been previously written to disk (via calling 
  writeTempFiles) into a single video file with given name.  */
  void encodeTempFiles(const std::string& fileName) const
  {
    // rsAssert(isFfmpegInstalled(), "This function requires ffmpeg to be installed");
    std::string cmd = getFfmpegInvocationCommand(fileName);
    std::cout << "Invoking ffmpeg.exe with command:\n";
    std::cout << cmd + "\n\n"; 
    system(cmd.c_str());
  }

  /** Deletes the temporary .ppm files that have been created during the process. */
  void deleteTempFiles(int numFrames) const 
  {
    std::cout << "Deleting temporary .ppm files\n";
    for(int i = 0; i < numFrames; i++)
      remove(getTempFileName(i).c_str());
    // maybe this should also show progress?
  }


  //-----------------------------------------------------------------------------------------------
  /** \name Internals */

  /** Creates the command string to call ffmpeg. */
  std::string getFfmpegInvocationCommand(const std::string& fileName) const
  {
    std::string cmd;
    cmd += "ffmpeg ";                                                 // invoke ffmpeg
    cmd += "-y ";                                                     // overwrite without asking
    cmd += "-r " + std::to_string(frameRate) + " ";                   // frame rate
    //cmd += "-f image2 ";                                              // ? input format ?
    //cmd += "-s " + std::to_string(w) + "x" + std::to_string(h) + " "; // pixel resolution
    cmd += "-i " + framePrefix + "%d.ppm ";                           // ? input data ?
    cmd += "-vcodec libx264 ";                                        // H.264 codec is common
    //cmd += "-vcodec libx265 ";                                        // H.265 codec is better
    cmd += "-crf " + std::to_string(compression) + " ";               // constant rate factor
    cmd += "-pix_fmt yuv420p ";                                       // yuv420p seems common
    cmd += "-preset veryslow ";                                       // best compression, slowest
    cmd += fileName + ".mp4";                                         // output file
    return cmd;

    // The command string has been adapted from here:
    // https://hamelot.io/visualization/using-ffmpeg-to-convert-a-set-of-images-into-a-video/
    // and by trial and error, i have commented out those options which turned out to be 
    // unnecessarry and added a few other options

    // Notes on the options:
    // -wihtout -pix_fmt yuv420p, it seems to use yuv444p by default and VLC player can't play it
    //  correctly (i think, the "p" stands for "predictive"?) - try it with ffprobe
  }

  /** Returns the name that should be used for the temp-file for a given frame. */
  std::string getTempFileName(int frameIndex) const
  {
    return framePrefix + std::to_string(frameIndex) + ".ppm";
  }


protected:

  // encoder settings:
  int compression = 0;   // 0: lossles, 51: worst
  int frameRate   = 25;

  std::string framePrefix = "VideoTempFrame";  
  // used for the temp files: VideoTempFrame1.ppm, VideoTempFrame2.ppm, ...

  // have strings for other options: pix_fmt, vcodec, preset:
  // std::string pix_fmt = "yuv420p";
  // std::string vcodec  = "libx264"
  // std::string preset  = "veryslow";


  bool cleanUp = true;

  rsConsoleProgressIndicator progressIndicator;

  //std::string tempDir, outDir, outFileName;
};
// todo: maybe allow to encode into different qualities using the same temporary files, i.e. create
// temp files once but run the encoder several times over the files with different quality 
// settings (lossless should be among them for reference)

// Infos about the video codecs of the H.26x family:
// H.261 (1988): https://en.wikipedia.org/wiki/H.261
// H.262 (1995): https://en.wikipedia.org/wiki/H.262/MPEG-2_Part_2
// H.263 (1996): https://en.wikipedia.org/wiki/H.263
// H.264 (2003): https://en.wikipedia.org/wiki/Advanced_Video_Coding
// H.265 (2013): https://en.wikipedia.org/wiki/High_Efficiency_Video_Coding
// H.266 (202x): https://en.wikipedia.org/wiki/Versatile_Video_Coding

// Guidelines for encoding video for youtube:
// https://support.google.com/youtube/answer/1722171?hl=en
// container format: .mp4
// video codec: H.264,
// audio codec: AAC-LC, stereo or 5.1, 48 kHz
// common frame rates: 24,25,30,48,50,60
// resolution: 16:9 aspect ratio is standard,

// about encoding losses:
// https://superuser.com/questions/649412/ffmpeg-x264-encode-settings
// https://slhck.info/video/2017/02/24/crf-guide.html





//=================================================================================================

/** Given 3 arrays of images for the red, green and blue color channels, this function writes them
as video into an .mp4 file. It is assumed that the arrays R,G,B have the same length and each image
in all 3 arrays should have the same pixel-size. The function first writes each frame as a separate
.ppm file into the current working directory (typically the project folder) and then invokes ffmpeg
to stitch the images together to a video file. For this to work (on windows), ffmpeg.exe must be 
either present in the current working directory (where the .ppm images end up) or it must be 
installed on the system and the PATH environment variable must contain the respective installation 
path. You may get it here: https://www.ffmpeg.org/ */
void writeToVideoFileMP4(
  const std::vector<rsImage<float>>& R,
  const std::vector<rsImage<float>>& G,
  const std::vector<rsImage<float>>& B,
  const std::string& name,                    // should not include the .mp4 suffix
  int frameRate = 25,
  int qualityLoss = 0)                        // 0: lossless, 51: worst
{
  size_t numFrames = R.size();
  rsAssert(G.size() == numFrames && B.size() == numFrames);
  if(numFrames == 0)
    return;

  // write each frame into a ppm file:
  int w = R[0].getWidth();                // pixel width
  int h = R[0].getHeight();               // pixel height
  std::string prefix = name + "_Frame";   // prefix for all image file names
  std::cout << "Writing ppm files\n\n";
  for(size_t i = 0; i < numFrames; i++)
  {
    std::string path = prefix + std::to_string(i) + ".ppm";
    rsAssert(R[i].hasShape(w, h)); 
    rsAssert(G[i].hasShape(w, h));
    rsAssert(B[i].hasShape(w, h));
    writeImageToFilePPM(R[i], G[i], B[i], path.c_str());
  }

  // invoke ffmpeg to convert the frame-images into a video (maybe factor out):

  // rsAssert(isFfmpegInstalled(), "This function requires ffmpeg to be installed");
  // compose the command string to call ffmpeg:
  std::string cmd;
  cmd += "ffmpeg ";                                                 // invoke ffmpeg
  cmd += "-y ";                                                     // overwrite without asking
  cmd += "-r " + std::to_string(frameRate) + " ";                   // frame rate
  //cmd += "-f image2 ";                                              // ? input format ?
  //cmd += "-s " + std::to_string(w) + "x" + std::to_string(h) + " "; // pixel resolution
  cmd += "-i " + prefix + "%d.ppm ";                                // ? input data ?
  cmd += "-vcodec libx264 ";                                        // very common standard
  //cmd += "-vcodec libx265 ";                                      // ...newer standard
  cmd += "-crf " + std::to_string(qualityLoss) + " ";               // constant rate factor
  //cmd += "-pix_fmt yuv420p ";                                       // seems to be standard
  //cmd += "-pix_fmt rgb24 ";                                       // ..this does not work well
  cmd += "-preset veryslow ";                                       // best compression, slowest
  cmd += name + ".mp4";                                             // output file


  // crf: quality - smaller is better, 0 is best, 20 is already quite bad, 10 seems good enough
  // see: https://trac.ffmpeg.org/wiki/Encode/H.264
  // The range of the CRF scale is 0–51, where 0 is lossless, 23 is the default, and 51 is worst 
  // quality possible. Even the lossless setting may achieve compression of factor 30 with the SIRP
  // model


  std::cout << "Invoking ffmpeg.exe with command:\n";
  std::cout << cmd + "\n\n"; 
  system(cmd.c_str());

  // maybe optionally clean up the (temporary) .ppm files - but the can be useful - one can "run"
  // the animation manually in IrfanView by using the cursor keys
}
// this function has quite a lot parameters - maybe about time to turn it into a class - the can be
// a convenience function, though
// ...but maybe keep this as a quick-and-dirty solution

// pixel-formats:
// -with yuv420p, VLC player can play it nicely but windows media player shows garbage
// -with rgb24, VLC player shows garbage and wnidows media player gives an error message and 
//  refuses to play at all
// -for the meaning of pixel-formats, see:
//  https://github.com/FFmpeg/FFmpeg/blob/master/libavutil/pixfmt.h

// for audio encoding using flac, see
// https://www.ffmpeg.org/ffmpeg-codecs.html#flac-2

// maybe try to encode with H.265 instead of H.264
// https://trac.ffmpeg.org/wiki/Encode/H.265
// this is a newer version of the encoder that may give better compression
// it seems to make no difference - using the option -vcodec libx265 instead of -vcodec libx265,
// i get exactly the same filesize

//=================================================================================================

/** Class for representing a parametric plane (parametrized by s and t) given in terms of 3 vectors
u,v,w:

   P(s,t) = u + s*v + t*u 
   
Aside from being able to compute points on the plane, it can also compute parameter pairs s,t such
that the point on the plane at these parameters is close to some given target point...  */

template<class T>
class rsParametricPlane3D
{


public:


  void setVectors(const rsVector3D<T>& newU, const rsVector3D<T>& newV, const rsVector3D<T>& newW)
  {
    u = newU;
    v = newV;
    w = newW;
  }

  /** Returns the point (x,y,z) on the plane that corresponds to the pair of parameters (s,t). */
  rsVector3D<T> getPointOnPlane(T s, T t) const
  {
    return u + s*v + t*w;
  }

  /** Computes parameters s,t such that a point on the plane with these parameters will be have 
  matched x and y coordinates with the given target point. */
  void getMatchParametersXY(const rsVector3D<T>& target, T* s, T* t) const
  {
    rsVector3D<T> c = target-u;
    T k =   1 / (v.y*w.x - v.x*w.y);
    *s  =  (c.y*w.x - c.x*w.y) * k;
    *t  = -(c.y*v.x - c.x*v.y) * k;
  }
  // var("s t cx cy vx vy wx wy")
  // e1 = cx == s*vx + t*wx
  // e2 = cy == s*vy + t*wy
  // solve([e1,e2],[s,t])
  // -> [[s == (cy*wx - cx*wy)/(vy*wx - vx*wy), t == -(cy*vx - cx*vy)/(vy*wx - vx*wy)]]

  /** Computes parameters s,t such that a point on the plane with these parameters will be have 
  matched x and z coordinates with the given target point. */
  void getMatchParametersXZ(const rsVector3D<T>& target, T* s, T* t) const
  {
    rsVector3D<T> c = target-u;
    T k =   1 / (v.z*w.x - v.x*w.z);
    *s =  (c.z*w.x - c.x*w.z) * k;
    *t = -(c.z*v.x - c.x*v.z) * k;
  }

  /** Computes parameters s,t such that a point on the plane with these parameters will be have 
  matched y and z coordinates with the given target point. */
  void getMatchParametersYZ(const rsVector3D<T>& target, T* s, T* t) const
  {
    rsVector3D<T> c = target-u;
    T k =   1 / (v.z*w.y - v.y*w.z);
    *s =  (c.z*w.y - c.y*w.z) * k;
    *t = -(c.z*v.y - c.y*v.z) * k;
  }

  /** Computes parameters s,t such that a point on the plane with these parameters will be closest 
  to the given target point in the sense of minimizing the Euclidean distance. */
  void getMatchParameters(const rsVector3D<T>& target, T* s, T* t) const
  {
    // Idea: minimize the squared norm of a-b where a = u + s*v + t*u is a vector produced by our 
    // plane equation and b is the target vector. This leads to finding the minimum of the 
    // quadratic form: err(s,t) = A*s^2 + B*s*t + C*t^2 + D*s + E*t + F with respect to s and t 
    // with the coefficients defined as below:

    rsVector3D<T> c = u-target;  // c := u-b -> now we minimize norm of c + s*v + t*w
    T A =   dot(v,v);
    T B = 2*dot(v,w);
    T C =   dot(w,w);
    T D = 2*dot(c,v);
    T E = 2*dot(c,w);
    T F =   dot(c,c);
    T k = 1 / (B*B - 4*A*C); // can we get a division by zero here?
    *s  = (2*C*D - B*E) * k;
    *t  = (2*A*E - B*D) * k;

    // Is it possible to derive simpler formulas? maybe involving the plane normal (which can be 
    // computed via the cross-product of v and w)? There are a lot of multiplications being done 
    // here....
  }
  // sage:
  // var("s t A B C D E F")
  // f(s,t) = A*s^2 + B*s*t + C*t^2 + D*s + E*t + F
  // dfds = diff(f, s);
  // dfdt = diff(f, t);
  // e1 = dfds == 0
  // e2 = dfdt == 0
  // solve([e1,e2],[s,t])
  // -> [[s == (2*C*D - B*E)/(B^2 - 4*A*C), t == -(B*D - 2*A*E)/(B^2 - 4*A*C)]]

  // symbolic optimization with sage
  // stationary_points = lambda f : solve([gi for gi in f.gradient()], f.variables())
  // f(x,y) = -(x * log(x) + y * log(y))
  // stationary_points(f)
  // [[x == e^(-1), y == e^(-1)]]
  // doesnt work here because we have all these parameters A,B,C,... and this code finds the 
  // gradient also with respct to these parameters, if we adapt it

  // stationary_points = lambda f : solve([gi for gi in f.gradient()], f.variables())

  // https://ask.sagemath.org/question/38079/can-sage-do-symbolic-optimization/


  /** Returns a point on the plane that is closest to the given target point in the sense of having
  the minimum Euclidean distance. */
  rsVector3D<T> getClosestPointOnPlane(const rsVector3D<T>& target) const
  {
    T s, t;
    getMatchParameters(target, &s, &t);
    return getPointOnPlane(s, t);
  }

  /** Computes the distance of the given point p from the plane. */
  T getDistance(const rsVector3D<T>& p) const
  {
    rsVector3D<T> q = getClosestPointOnPlane(p);
    q -= p;
    return q.getEuclideanNorm();
  }
  // needs test
  // todo: compute the signed distance - maybe that requires translating to implicit form:
  //   A*x + B*y + C*z + D = 0


protected:

  rsVector3D<T> u, v, w;

};
// todo: clean up and move to rapt (into Math/Geometry)


/** Returns a vector that contains a chunk of the given input vector v, starting at index "start" 
with length given by "length". */
template<class T>
inline std::vector<T> rsChunk(const std::vector<T>& v, int start, int length)
{
  rsAssert(length >= 0);
  rsAssert(start + length <= (int) v.size());
  std::vector<T> r(length);
  rsArrayTools::copy(&v[start], &r[0], length);
  return r;
}

//-------------------------------------------------------------------------------------------------

/** Extends rsMultiArray by storing information whether a given index is covariant or contravariant
and a tensor weight which is zero for absolute tensors and nonzero for relative tensors. 

A tensor is:
-A geometric or physical entity that can be represented by a multidimensional array, once a 
 coordinate system has been selected. The tensor itself exists independently from the coordinate 
 systems, but its components (i.e. the array elements) are always to be understood with respect to 
 a given coordinate system.
-A multilinear map that takes some number of vectors and/or covectors as inputs and produces a 
 scalar as output. Multilinear means that it is linear in all of its arguments.
-When converting tensor components from one coordinate system to another, they transform according
 to a specific set of transformation rules.
-Scalars, vectors (column-vectors), covectors (row-vectors) and matrices are included as special 
 cases, so tensors really provide a quite general framework for geometric and physical 
 calculations.

 References:
   (1) Principles of Tensor Calculus (Taha Sochi)


*/

template<class T>
class rsTensor : public rsMultiArray<T>
{

public:

  // todo: setters

  using rsMultiArray::rsMultiArray;  // inherit constructors


  //-----------------------------------------------------------------------------------------------
  // \name Setup


  //-----------------------------------------------------------------------------------------------
  // \name Inquiry

  int getRank() const { return getNumIndices(); }
  // this is the total rank, todo: getCovariantRank, getContravariantRank

  bool isIndexCovariant(int i) const { return covariant[i]; }

  bool isIndexContravariant(int i) const { return !isIndexCovariant(i); }

  int getWeight() const { return weight; }

  /** Returns true, iff indices i and j are of opposite variance type, i.e. one covariant the other
  contravariant. This condition is required for the contraction with respect to i,j to make 
  mathematical sense, so it can be used as sanity check to catch errors in tensor computations. */
  bool areIndicesOpposite(int i, int j) const
  {
    return (covariant[i] && !covariant[j]) || (!covariant[i] && covariant[j]);
  }

  // isScalar, isVector, isCovector, 

  // compares ranks, shape, variances and weights
  bool isOfSameTypeAs(const rsTensor<T>& A) const
  { return this->shape == A.shape && this->covariant == A.covariant && this->weight == A.weight; }

  /** Compares this tensor with another tensor A for equality with a tolerance. */
  bool equals(const rsTensor<T>& A, T tol) const
  {
    bool r = true;
    r &= getShape() == A.getShape();
    r &= covariant  == A.covariant;
    r &= weight     == A.weight;
    r &= rsArrayTools::almostEqual(getDataPointerConst(), A.getDataPointerConst(), getSize(), tol);
    return r;
  }

  //-----------------------------------------------------------------------------------------------
  // \name Operations
  // see rsMultiArrayOld for possible implementations

  /** Returns a tensor that results from contracting tensor A with respect to the given pair of 
  indices.  */
  static rsTensor<T> getContraction(const rsTensor<T>& A, int i, int j)
  {
    // sanity checks:
    rsAssert(A.covariant.size() == 0 || A.areIndicesOpposite(i, j),
      "Indices i,j must have opposite (co/contra) variance type for contraction");
    rsAssert(A.shape[i] == A.shape[j], "Summation indices must have the same range");
    rsAssert(A.getRank() >= 2, "Rank must be at least 2");

    // verify this:
    rsTensor<T> B;
    B.shape.resize(A.shape.size() - 2);
    int k = 0, l = 0;
    while(k < B.getRank()) {
      if(l == i || l == j)  {
        l++; continue; }
      B.shape[k] = A.shape[l];
      k++; l++; }
    B.adjustToNewShape();

    // this needs thorough verifications - step/jump/start were mostly guessed:
    int step = A.strides[i] + A.strides[j];
    int jump = rsArrayTools::sum(&A.strides[0], (int) A.strides.size()) - step;
    int num  = A.shape[i];                    // == A.shape[j]
    for(k = 0; k < B.getSize(); k++) {
      B.data[k] = T(0);
      int start = k*jump;
      for(l = 0; l < num; l++)
        B.data[k] += A.data[start + l*step]; }
    return B;
  }


  /** Computes the outer product between tensors A and B, also known as direct product, tensor 
  product or Kronecker product. Every element of tensor A is multiplied with every element from 
  tensor B. todo: explain, how the data is arranged in the result */
  static rsTensor<T> getOuterProduct(const rsTensor<T>& A, const rsTensor<T>& B)
  {
    // Allocate and set up result:
    size_t i;
    rsTensor<T> C;
    C.shape.resize(A.shape.size() + B.shape.size());
    for(i = 0; i < A.shape.size(); i++) C.shape[i]                  = A.shape[i];
    for(i = 0; i < B.shape.size(); i++) C.shape[i + A.shape.size()] = B.shape[i];
    C.adjustToNewShape();

    // Set up the weight and the "covariant" array of the result:
    C.weight = A.weight + B.weight;  // verify
    C.covariant.resize(A.covariant.size() + B.covariant.size());
    for(i = 0; i < A.covariant.size(); i++) C.covariant[i]                      = A.covariant[i];
    for(i = 0; i < B.covariant.size(); i++) C.covariant[i + A.covariant.size()] = B.covariant[i];

    // Fill the data-array of the result and return it:
    int k = 0;
    for(int i = 0; i < A.getSize(); i++) {
      for(int j = 0; j < B.getSize(); j++) {
        C.data[k] = A.data[i] * B.data[j]; k++; }}
    return C;
  }

  static rsTensor<T> getInnerProduct(const rsTensor<T>& A, const rsTensor<T>& B, int i, int j)
  {
    return getContraction(getOuterProduct(A, B), i, j);  
    // preliminary - todo: optimize this by avoiding the blown up outer product as intermediate
    // result
  }


  static int getDivisionIndex(const rsTensor<T>& A) // maybe rename to getBestDivisorIndex
  {
    //return 0;  // preliminary
    //return 7;  // test
    //return 5; // test
    //return rsArrayTools::firstIndexWithNonZeroValue(A.getDataPointerConst(), A.getSize());
    return rsArrayTools::maxAbsIndex(A.getDataPointerConst(), A.getSize());
      // maybe later use an entry that causes the least rounding errors (i think, we should look 
      // for numbers, whose mantissa has the largest number trailing zero bits) - in case of 
      // complex numbers we should use the one for which the minimum of both re/im of the number
      // of trailing mantissa bits is largest
  }

  /** Given a tensor product C = A*B and the right factor B, this function retrieves the left
  factor A. */
  static rsTensor<T> getLeftFactor(const rsTensor<T>& C, const rsTensor<T>& B)
  {
    rsTensor<T> A;      // result
    int rankA = C.getRank() - B.getRank();
    A.setShape(rsChunk(C.shape, 0, rankA));
    if(C.covariant.size() > 0)
      A.covariant = rsChunk(C.covariant, 0, rankA);
    A.weight = C.weight - B.weight;

    int offset = getDivisionIndex(B);
    int k = B.getSize();
    for(int i = 0; i < A.getSize(); i++)
      A.data[i] = C.data[k*i + offset] / B.data[offset];
    return A;
  }
  // seems to work

  /** Given a tensor product C = A*B and the left factor A, this function retrieves the right 
  factor B. */
  static rsTensor<T> getRightFactor(const rsTensor<T>& C, const rsTensor<T>& A)
  {
    rsTensor<T> B;      // result
    int rankA = A.getRank();
    int rankB = C.getRank() - rankA;
    B.setShape(rsChunk(C.shape, rankA, rankB));
    if(C.covariant.size() > 0)
      B.covariant = rsChunk(C.covariant, rankA, rankB);
    B.weight = C.weight - A.weight;

    // needs verification:
    int offset = getDivisionIndex(A);
    int k = B.getSize();
    for(int i = 0; i < B.getSize(); i++)
      B.data[i] = C.data[i + k*offset] / A.data[offset];
    return B;
  }
  // seems to work



  //-----------------------------------------------------------------------------------------------
  // Factory functions:

  /** Creates the Kronecker delta tensor (aka unit tensor) for the given number of dimensions. This
  is a rank-2 tensor represented by the NxN identity matrix. */
  static rsTensor<T> getDeltaTensor(int numDimensions)
  {
    int N = numDimensions;
    rsTensor<T> D;
    D.setShape({N, N});
    D.setToZero();
    for(int i = 0; i < N; i++)
      D(i, i) = T(1);
    return D;
  }
  // see (1) pg. 76
  // todo: create generalized delta tensor - maybe use an optional parameter...
  // maybe rename to getUnitTensor

  /** Returns the permutation tensor for the given number of dimensions. It's also known as 
  epsilon tensor, Levi-Civita tensor, anti-symmetric and alternating tensor. It has a rank 
  equal to the number of dimensions, so if this number is N, this tensor has N^N components, which 
  means it grows really fast with the number of dimensions. It's very sparse though, i.e. many 
  components are zero, so it's probably not a good idea to use this tensor explicitly in 
  computations in production code - especially in higher dimensions. 
  ...todo: use weight...set it in the tensor and fill the "covariant" array accordingly
  */
  static rsTensor<T> getPermutationTensor(int numDimensions, int weight = 0)
  {
    int N = numDimensions;
    std::vector<int> indices(N);  
    rsFill(indices, N);             // we "abuse" the indices array here to represent the shape
    rsTensor<T> E(indices);
    E.setToZero();
    for(int i = 0; i < E.getSize(); i++)
    {
      E.structuredIndices(i, &indices[0]);
      E.data[i] = (T) rsLeviCivita(&indices[0], N);
      // this implementation may still be highly suboptimal - i just wanted something that works
      int dummy = 0;
    }

    // maybe factor out, so we may have a version that doesn't use the weight and variance-flags:
    E.weight = weight;
    E.covariant.resize(N);
    if(weight == -1)
      rsFill(E.covariant, char(1));
    else
      rsFill(E.covariant, char(0));


    return E;
  }
  // see (1) pg 77
  // it's either a contravariant relative tensor of weight +1 or a covariant tensor of weight -1
  // (see (1) pg. 81) -> set this up correctly! ...maybe optionally - pass -1 or +1 as parameter - 
  // or 0, if the weight should not be used





  static rsTensor<T> getGeneralizedDeltaTensor(int numDimensions)
  {
    int N = numDimensions;
    rsTensor<T> Eu = getPermutationTensor(N, +1); // contravariant version of permutation tensor
    rsTensor<T> El = getPermutationTensor(N, -1); // covariant version
    return Eu * El;                               // (1) Eq. 201
  }
  // it's a mixed tensor - the first N indices are upper the second N indices are lower, (1) Eq 201



  

  // todo:
  // -factory functions for special tensors: epsilon, delta (both with optional argument to produce
  //  the generalized versions
  //  -maybe verify some epsilon-delta identities in unit test
  // -(anti)symmetrizations

  //-----------------------------------------------------------------------------------------------
  // Operators:

  rsTensor<T> operator+(const rsTensor<T>& B) const
  { 
    rsAssert(isOfSameTypeAs(B), "Tensors to be added must have same shape, variances and weights");
    rsTensor<T> C(this->shape); 
    this->add(*this, B, &C); 
    return C; 
  }

  rsTensor<T> operator-(const rsTensor<T>& B) const
  { 
    rsAssert(isOfSameTypeAs(B), "Tensors to be subtracted must have same shape, variances and weights");
    rsTensor<T> C(this->shape); 
    this->subtract(*this, B, &C); 
    return C; 
  }

  rsTensor<T> operator*(const rsTensor<T>& B) const
  { return getOuterProduct(*this, B); }

  rsTensor<T> operator/(const rsTensor<T>& B) const
  { return getLeftFactor(*this, B); }

  // maybe we should override the multiplication operator? the baseclass defines it as element-wise
  // multiplication...but then, maybe we should also override the division - but should be then 
  // return the left or the right factor? is one more common than the other? what about the
  // quotient-rule (Eq. 141) - in this case, C and B are given and A is computed....but no - this
  // rule involves a contraction...i think, when starting from A*B = C, then dividing both sides by 
  // B leading to A = A*B/B = C/B is more natural than dividing both sides by a and writing
  // B = A*B/A = C/A, so we should probably compute A - the left factor - or maybe disallow 
  // division by the / operator...or see what sage or matblab or mathematica do

  // todo: multiplication by a scalar (outside the class)

  rsTensor<T> operator==(const rsTensor<T>& B) const
  {
    return this->isOfSameTypeAs(B) 
      && rsArrayTools::equal(this->getDataPointer(), B.getDataPointer(), getSize());
  }

protected:

  void adjustToNewShape()
  {
    updateStrides(); 
    updateSize();
    data.resize(getSize());
    updateDataPointer();
  }
  // maybe move to baseclass

  // using these is optional - if used, it allows for some sanity checks in the arithmetic 
  // operations such as catching (i.e. raising an assertion) when you try to add tensors of 
  // different variance types and/or weight or contracting with respect to two indices of the same
  // variance type (none of which makes sense mathematically):
  int weight = 0;


  //std::vector<bool> covariant; // true if an index is covariant, false if contravariant
  std::vector<char> covariant; // 1 if an index is covariant, 0 if contravariant
    // todo: use a more efficient datastructure - maybe rsFlags64 - this would limit us to tensors
    // up to rank 64 - but that should be crazily more than enough

  //A.covariant = rsChunk(C.covariant, 0, rankA);
  // doesn't compile with vector<bool> because:
  // https://stackoverflow.com/questions/17794569/why-is-vectorbool-not-a-stl-container
  // https://howardhinnant.github.io/onvectorbool.html
  // that's why i use vector<char> for the time being - eventually, it's perhaps best to use 
  // rsFlags64 (which may have to be written)
};

/** Multiplies a scalar and a tensor. */
template<class T>
inline rsTensor<T> operator*(const T& s, const rsTensor<T>& A)
{ rsTensor<T> B(A); B.scale(s); return B; }


//-------------------------------------------------------------------------------------------------

/** Class for doing computations with N-dimensional manifolds that are embedded in M-dimensional
Euclidean space, where M >= N. The user must provide a function that takes as input an 
N-dimensional vector of general curvilinear coordinates and produces a corresponding M-dimensional 
vector of the resulting coordinates in the embedding space. The case N = M = 3 can be used to do 
work with cylindrical or spherical coordinates in 3D space. The class can compute various geometric
entities on the manifold, such as the metric tensor...

todo: compute lengths of curves, areas, angles, geodesics, ...

References:
  (1) Principles of Tensor Calculus (Taha Sochi)
  (2) Aufstieg zu den Einsteingleichungen (Michael Ruhrländer)


todo: 
-all numerical derivatives are computed using a central difference approximation and they all use
 the same stepsize h (which can be set up from client code) - maybe let the user configure the 
 object with some sort of rsNumercialDifferentiator where such things can be customized
-avoid excessive memory allocations: maybe pre-allocate some temporary vectors, matrices and 
 higher rank tensors which are used over and over again for intermediate results instead of 
 allocating them as local variables

*/

template<class T>
class rsManifold
{

public:

  rsManifold(int numManifoldDimensions, int numEmbeddingSpaceDimensions)
  {
    N = numManifoldDimensions;
    M = numEmbeddingSpaceDimensions;

    rsAssert(M >= N, "Can't embedd manifolds in lower dimensional spaces");
    // ...you may consider to recast your problem as an M-dimensional manifold in N-space, i.e. 
    // swap the roles of M and N
  }


  /** An array of N functions where each functions expects N arguments, passed as length-N vector, 
  where N is the dimensionality of the space. */
  //using FuncArray = std::vector<std::function<T(const std::vector<T>&)>>;

  using Vec  = std::vector<T>;
  using Mat  = rsMatrix<T>;
  using Tens = rsMultiArray<T>;

  using FuncSclToVec = std::function<void(T, Vec&)>;
  // function that maps scalars to vectors, i.e defines parametric curves

  using FuncVecToVec = std::function<void(const Vec&, Vec&)>;
  // 1st argument: input vector, 2nd argument: output vector

  using FuncVecToMat = std::function<void(const Vec&, Mat&)>;
  // 1st argument: input vector, 2nd argument: output Jacobian matrix

  //-----------------------------------------------------------------------------------------------
  // \name Setup


  /** Sets the approximation stepsize for computing numerical derivatives. Client code may tweak 
  this for more accurate results. */
  void setApproximationStepSize(T newSize) { h = newSize; }

  /** Sets the functions that convert from general curvilinear coordinates to the corresponding 
  canonical cartesian coordinates. */
  void setCurvilinearToCartesian(const FuncVecToVec& newCoordFunc)
  { u2x = newCoordFunc; }


  /** Sets the functions that convert from canonical cartesian coordinates to the corresponding 
  general curvilinear coordinates.. */
  void setCartesianToCurvilinear(const FuncVecToVec& newInverseCoordFunc)
  { x2u = newInverseCoordFunc; }
  // this should be optional, too


  // all stuff below is optional...

  void setCurvToCartJacobian(const FuncVecToMat& newFunc)
  { u2xJ = newFunc; }

  void setCartToCurvJacobian(const FuncVecToMat& newFunc)
  { x2uJ = newFunc; }



  //-----------------------------------------------------------------------------------------------
  // \name Computations

  /** Converts a vector of general curvilinear coordinates to the corresponding canonical cartesian
  coordinates. */
  void toCartesian(const Vec& u, Vec& x) const 
  { checkDims(u, x); u2x(u, x); }
  // x should be M-dim, u should be N-dim

  /** Converts a vector of canonical cartesian coordinates to the corresponding general curvilinear
  coordinates. */
  void toCurvilinear(const Vec& x, Vec& u) const 
  { checkDims(u, x); x2u(x, u); }
  // x should be M-dim, u should be N-dim


  /** The i-th column of the returned matrix E is the i-th covariant basis vector emanating from a 
  given (contravariant) position vector u. E(i,j) is the j-th component of the i-th basis 
  vector. */
  Mat getCovariantBasis(const Vec& u) const
  {
    rsAssert((int)u.size() == N);
    if( u2xJ ) {  // is this the right way to check, if std::function is not empty?
      Mat E(M, N); u2xJ(u, E); return E; }
    else
      return getCovariantBasisNumerically(u);
  }







  /** Returns MxN matrix, whose columns are the covariant(?) basis vectors in R^M at the 
  contravariant(?) coordinate position u in R^N. */
  Mat getCovariantBasisNumerically(const Vec& u) const
  {
    Vec x(M);

    // Compute tangents of the coordinate curves at x - these are the basis vectors - and 
    // organize them in a MxN matrix:
    Mat E(M, N);
    Vec up = u, um = u;   // u with one of the coordinates wiggled u+, u-
    Vec xp = x, xm = x;   // resulting wiggled x-vectors - why init with an uninitialized x?
    T s = 1/(2*h);
    for(int j = 0; j < N; j++)
    {
      // wiggle one of the u-coordinates up and down:
      up[j] = u[j] + h;    // u+
      um[j] = u[j] - h;    // u-

      // compute resulting x-vectors:
      toCartesian(up, xp); // x+
      toCartesian(um, xm); // x-

      // approximate tangent by central difference:
      x = s * (xp - xm); // maybe this can be get rid of - do it directly in the loop below..

      // write tangent into matrix column:
      //E.fillColumn(i, &x[0]);
      for(int i = 0; i < M; i++)
        E(i, j) = x[i];  // we could do directly E(i, j) = s * (xp[i] - xm[i])

      // restore u-coordinates:
      up[j] = u[j];
      um[j] = u[j];
    }

    return E; // E = [ e_1, e_2, ..., e_N ], the columns are the basis vectors
  }
  // maybe, if we express this stuff in terms of tensors, we can also check, if the incoming vector
  // is a (1,0)-tensor (as opposed to being a (0,1)-tensor) - the index should be a contravariant, 
  // i.e. upper index.
  // wait - isn't this matrix here the Jacobian matrix? see Eq 64
  // maybe rename to getCoordinateCurveTangents - see Eq. 45
  // wait - is it consistent to use the columns as basis vectors? column-vectors are supposed to
  // be contravariant vectors, but these basis-vectors are covariant. On the other hand, when 
  // forming a vector as linear combination from these basis vectors, we use contravariant 
  // components and these should multiply column-vectors such that the result is also a column 
  // vector

  Mat getContravariantBasis(const Vec& u) const
  {
    rsAssert((int)u.size() == N);
    if( x2uJ ) {  // is this the right way to check, if std::function is not empty?
      Mat E(N, M); Vec x(M); u2x(u, x); x2uJ(x, E); return E; }
    else
      return getContravariantBasisNumerically(u);
  }

  void fillCovariantBasisVector(const Vec& u, int i, Vec& Ei)
  {
    Vec up = u, um = u;    // u with one of the coordinates wiggled u+, u-
    Vec xp = x, xm = x;    // resulting wiggled x-vectors
    up[i] = u[i] + h;      // u+
    um[i] = u[i] - h;      // u-
    toCartesian(up, xp);   // x+
    toCartesian(um, xm);   // x-
    Ei = (1/(2*h)) * (xp - xm) ;
  }




  /*
  rsMatrix<T> getContravariantBasis(const std::vector<T>& u) const
  {
    rsAssert((int)u.size() == N);

    return getContravariantBasisNumerically(u);
  }
  */


  /** Computes the contravariant basis vectors according to their definition in (1), Eq. 45 as the
  gradients of the N u-functions as functions of the cartesian x-coordinates. Each of the N 
  u-functions can be seen as a scalar function of the M coordinates of the embedding space. The 
  gradients of the scalar fields in M-space are the contravariant basis vectors. The basis vectors 
  are the rows of the returned matrix - this is consistent with the idea that a gradient is 
  actually not a regular (column) vector but a covector (i.e. row-vector, aka 1-form). So the 
  function will return an NxM matrix whose rows are the contravariant basis vectors. The input 
  vector u is assumed to be in contravariant form as well (as usual). */
  Mat getContravariantBasisNumerically(const Vec& u) const
  {
    rsAssert((int)u.size() == N);
    Vec x(M);
    toCartesian(u, x); 
    T s = 1/(2*h);
    Vec xp = x, xm = x;   // wiggled x-vectors
    Vec up(N), um(N);
    Mat E(N, M);
    for(int j = 0; j < M; j++)
    {
      // wiggle one of the x-coordinates up and down:
      xp[j] = x[j] + h;    // x+
      xm[j] = x[j] - h;    // x-
      // this may actually produces invalid coordinates that are not inside the manifold - may this
      // be the reason, that things go wrong with gl*gu != id, i.e. the co-and contravarinat metric 
      // tensors do not multiply to the identity? ...chack with metrics computed from analytic 
      // Jacobians - but wait - i did this by hand - mayb it was wrong to throw away the z coordinate
      // in the computation of u,v

      // compute resulting u-vectors:
      toCurvilinear(xp, up); // u+
      toCurvilinear(xm, um); // u-

      // compute partial derivatives of the N u-coordinates to the j-th x-coordinate and write them
      // into the matrix:
      for(int i = 0; i < N; i++)
        E(i, j) = s * (up[i] - um[i]);

      // restore x-coordinates:
      xp[j] = x[j];
      xm[j] = x[j];
    }
    return E;
  }
  // factor out into a getJacobian - should be in rsNumericDifferentiator
  // maybe rename to getSpatialCoordinateGradients
  // i think the gradient of a scalar function should indeed be represented as a covector, i.e. the
  // matrix returned from here should be NxM and not MxN - such that the rows of the matrix are 
  // the individual gradients of the individual scalar functions u1(x1,...,xM),...,uN(x1,...,xM)
  // an alternative way to compute the contravariant basis vectors (that does not require client
  // code to provide the inverse coordinate mapping) would be to use the covariant basis vectors 
  // and raise the indices using the metric (perhaps the inverse metric?) ...maybe that could be 
  // used as fallback strategy when the inverse mapping is not provided? It would be really nice, 
  // if we could free client code from the requirement of providing the inverse functions, as they 
  // may be hard to obtain. All we really need is the ability to do a local inversion which is 
  // provided by inverting the Jacobian (right?)
  // when x2u is not assigned (empty): use as fallback: getCovariantBasis() and raise indices using
  // the metric


  /** Returns the covariant form of the metric tensor at given contravariant(?) curvilinear 
  coordinate position u. The metric tensor is a symmetric NxN matrix where N is the dimensionality
  of the manifold. */
  Mat getCovariantMetric(const Vec& u) const
  {
    Mat E = getCovariantBasis(u);
    return E.getTranspose() * E;  // (1), Eq. 213 (also Eq. 62 ? with E == J)
  }
  // maybe the computation of the metric can be done more efficiently using the symmetries (it is a
  // symmetric matrix)? in this case, we should not rely on first computing the basis vectors 
  // because the matrix of basis vectors is not necessarily symmetric
  // maybe Eq 216,218 - but no - we actually need all M*N partial derivatives - the symmetry 
  // results from the commutativity of multiplication of scalars - the entries are products of
  // two partial derivatives

  Mat getContravariantMetric(const Vec& u) const
  {
    Mat E = getContravariantBasis(u);
    return E * E.getTranspose();  // (1), Eq. 214
  }
  // when x2u is not assigned (empty): use as fallback: getCovariantMetric().getInverse() 


  /** Computes the mixed dot product of a covariant covector and contravariant vector or vice 
  versa. No metric is involved, so you don't need to give a position vector u - the mixed metric 
  tensor is always the identity matrix. */
  static T getMixedDotProduct(const Vec& a, const Vec& b)
  {
    rsAssert(a.size() == b.size());
    return rsArrayTools::sumOfProducts(&a[0], &b[0], (int) a.size()); // (1), Eq 259,260
  }

  /** Computes the dot-product between two vectors (or covectors) with a given metric (which should
  be in contravariant form in the case of two vectors and in covariant form in the case of two 
  covectors...or the other way around?...figure out!) */
  static T getDotProduct(const Vec& a, const Vec& b, const Mat& g)
  {
    int N = (int)a.size();
    rsAssert(a.size() == b.size());
    //rsAssert(g.hasShape(N, N));
    T sum = T(0);
    for(int i = 0; i < N; i++)
      for(int j = 0; j < N; j++)
        sum += g(i, j) * a[i] * b[j];   // (1), Eq 257,258
    return sum;
  }

  /** Computes the dot-product of two contravariant vectors (i.e. regular column-vectors) emanating 
  from position vector u (using the covariant metric at u). */
  T getContravariantDotProduct(const Vec& a, const Vec& b, const Vec& u) const
  {
    Mat g = getContravariantMetric(u);    // g^ij
    return getDotProduct(a, b, g);
  }

  /** Computes the dot-product of two covariant vectors (aka row-vectors, covectors, 1-forms) 
  emanating from position vector u (using the contravariant metric at u). */
  T getCovariantDotProduct(const Vec& a, const Vec& b, const Vec& u) const
  {
    rsMatrix<T> g = getCovariantMetric(u);       // g_ij
    return getDotProduct(a, b, g);
  }

  // position vectors like u are always supposed to be given by contravariant components, right?
  // (1) pg 60: gradients of scalar fields are covariant, displacement vectors are contravariant
  // ...and a displacement should have the same variance as a position, i think - because it is
  // typically being added to a position

  /** Shifts (raises or lowers) the index of a vector with the given metric tensor. */
  Vec shiftVectorIndex(const Vec& a, const Vec& g) const
  {
    Vec r(N);         // result
    for(int i = 0; i < N; i++) {
      r[i] = T(0);
      for(int j = 0; j < N; j++)
        r[i] += a[j] * g(j, i); }     // (1), Eq. 228,229
  }
  // maybe make static - infer N from inputs


  /** Given a covariant covector (with lower index), this function returns the corresponding
  contravariant vector (with upper index). */
  Vec raiseVectorIndex(const Vec& a, const Vec& u) const
  {
    rsAssert((int) a.size() == N);
    Mat g = getContravariantMetric(u); // g^ij
    return shiftVectorIndex(a, g);
  }
  // needs test

  Vec lowerVectorIndex(const Vec& a, const Vec& u) const
  {
    rsAssert((int) a.size() == N);
    Mat g = getCovariantMetric(u);  // g_ij
    return shiftVectorIndex(a, g);
  }
  // needs test


  Mat raiseFirstMatrixIndex(const Mat& A, const Vec& u) const
  {
    //rsAssert(A.hasShape(N,N));  // should we allow NxM or MxN matrices?
    Mat g = getContravariantMetric(u); // g^ij

    // factor out into shiftFirstMatrixIndex:
    Mat B(N, N);
    B.setToZero();
    for(int i = 0; i < N; i++)
      for(int j = 0; j < N; j++)
        for(int k = 0; k < N; k++)
          B(i,j) += g(i,k) * A(k,j);  // (1), Eq 231,2
    return B;
  }
  // needs test

  Mat raiseSecondMatrixIndex(const Mat& A, const Vec& u) const
  {
    rsAssert(A.hasShape(N,N));  // should we allow NxM or MxN matrices?
    Mat g = getContravariantMetric(u); // g^ij

    // factor out into shiftFirstMatrixIndex:
    Mat B(N, N);
    B.setToZero();
    for(int i = 0; i < N; i++)
      for(int j = 0; j < N; j++)
        for(int k = 0; k < N; k++)
          B(j,i) += g(i,k) * A(j,k);  // (1), Eq 231,1
    return B;
  }
  // needs test

  // todo: implement also Eq 230,231 - raise one index in turn - starting from first


  /** Computes the Christoffel-symbols of the first kind [ij,l] and returns them as NxNxN 
  rsMultiArray. The (i,j,l)th element is accessed via C(i,j,l) when C is the multiarray of the
  Christoffel symbols. ...these are mainly used as an intermediate step to calculate the more 
  important Christoffel symbols of the second kind - see below...  */
  Tens getChristoffelSymbols1stKind(const Vec& u) const
  {
    int i, j, l;

    // Create an array of the partial derivatives of the metric with respect to the coordinates:
    Vec up(u), um(u);               // wiggled u coordinate vectors
    Mat gp, gm;                     // metric at up and um
    Mat g = getCovariantMetric(u);  // metric at u
    std::vector<Mat> dg(N);         // we need N matrices
    T s = 1/(2*h);
    for(i = 0; i < N; i++)
    {
      up[i] = u[i] + h;
      um[i] = u[i] - h;

      gp = getCovariantMetric(up);  // metric at up
      gm = getCovariantMetric(um);  // metric at um

      dg[i] = s * (gp - gm);        // central difference approximation

      up[i] = u[i];
      um[i] = u[i];
    }

    // Compute the Christoffel-symbols from the partial derivatives of the metric and collect them
    // into an NxNxN 3D array:
    Tens C({N,N,N}); 
    for(i = 0; i < N; i++)
      for(j = 0; j < N; j++)
        for(l = 0; l < N; l++)
          C(i,j,l) = T(0.5) * (dg[j](i,l) + dg[i](j,l) - dg[l](i,j)); // (1), Eq. 307
    return C;
  }
  // the notation G_kij is also used - 3 lowercase indices
  // https://en.wikipedia.org/wiki/List_of_formulas_in_Riemannian_geometry

  /** Computes the Christoffel symbol of the second kind G^k_ij at given position u. G for the  
  commonly used uppercase gamma with the 1st index upstairs, 2nd and 3rd downstairs. It's not 
  actually a tensor, but it's commonly denoted in this way - so the up/down notation does not 
  actually mean contra- or covariant in this case. The element G(k,i,j) has the following 
  meaning: In an n-dimensional space with n covariant basis vectors E_1,...,E_n which span the 
  space, the partial derivative E_i,j of the i-th basis vector with respect to the j-th coordinate
  is itself a vector living the same space, so it can be represented as a linear combination of the
  original basis vectors E_1,...,E_n. The coefficients in this linear combination are the 
  Christoffel symbols of the 2nd kind. They have 3 indices: i,j to indicate which basis vector is 
  differentiated and with respect to which coordinate (denoted downstairs) and k to denote, which 
  basis-vector it multiplies in the linear combination (denoted upstairs). The utility of the 
  Christoffel symbols is that they facilitate the calculation of the covariant derivative without
  resorting to any reference to the basis vectors of the tangent space at u in the embedding space. 
  By some magic cancellation of terms, it happens that they may be calculated using derivatives of 
  the metric tensor alone, which is an intrinsic property of the manifold, as opposed to the 
  tangent-space basis vectors which are extrinsic to the manifold. They are symmetric in their 
  lower indices, i.e. G^k_ij = G^k_ji because each of the lower indices refers to one partial 
  differentiation of an input position vector with respect to one of the original coordinates 
  (the basis-vectors are first derivatives of the coordinates and here we take (some symmetric 
  combination of) derivatives of the basis-vectors, so we get second derivatives) and the order of 
  partial derivatives is irrelevant (by Schwarz's theorem). */
  Tens getChristoffelSymbols2ndKind(const Vec& u) const
  {
    int k, i, j;
    // k: derivative index
    // i: basis vector index
    // j: vector component index
    // ...verify these....

    Mat g = getContravariantMetric(u);  // contravariant metric at u
    Tens C = getChristoffelSymbols1stKind(u);
    Tens G({N,N,N});
    G.setToZero();
    for(i = 0; i < N; i++)
      for(j = 0; j < N; j++)
        for(k = 0; k < N; k++)
        {
          for(int l = 0; l < N; l++)
          {
            G(k,i,j) += g(k,l) * C(i,j,l);  // (1), Eq. 308
            // or should we swap i,k - because the k in the formula is notated upstairs - it feels
            // more natural, if it's the first index - done - here, the upper index is denoted 
            // first:
            // https://en.wikipedia.org/wiki/Christoffel_symbols#Christoffel_symbols_of_the_first_kind
          }
        }
    return G;
  }
  // this is not a good algorithm - it involves the contravariant metric and has a quadruple-loop
  // ...is there a more direct way? probably yes! at least, it seems to give correct results

  /** Computes the matrix D of partial derivatives of the given vector field v = f(u) with respect 
  to the coordinates. D(i,j) is the j-th component of the partial derivative with respect to 
  coordinate i. */
  Mat getVectorFieldDerivative(const Vec& u, const FuncVecToVec& f) const
  {
    int i, j;
    // i: index of coordinate with respect to which partial derivative is taken
    // j: index of vector component in vector field

    // compute matrix of partial derivatives of the vector field f(u):
    Vec up(u), um(u);        // wiggled u coordinate vectors
    Vec Ap(N), Am(N);        // A = f(u) at up and um
    Mat D(N, N);
    T s = 1/(2*h);
    for(i = 0; i < N; i++)
    {
      up[i] = u[i] + h;
      um[i] = u[i] - h;

      // Compute vector fields at wiggled coordinates:
      f(up, Ap);
      f(um, Am);

      // Approximate partial derivatives by central difference and store them into matrix:
      for(j = 0; j < N; j++)
        D(i, j) = s * (Ap[j] - Am[j]);

      up[i] = u[i];
      um[i] = u[i];
    }

    return D;
  }
  // could actually be a static function - oh - no - it uses our member h




  /** Given two vector fields f = f(u) and g = g(u), this function computes the Lie bracket of 
  these vector fields at some given position vector u. The Lie bracket of f and g is just 
  f(g) - g(f), i.e. the commutator of the two vector fields. */
  Vec getLieBracket(const Vec& u, const FuncVecToVec& f, const FuncVecToVec& g) const
  {
    return f(g(u)) - g(f(u));
  }
  // verify this


  // we may also need a covariant derivative along a given direction - is this analoguous to the
  // directional derivative, as in: the scalar product of the gradient with a given vector? should
  // we form the scalar product of the covariant derivative with a given vector? ...figure out!

  // getTorsionTensor(const Vec& u, const FuncVecToVec& f, const FuncVecToVec& g)
  // torsion tensor of two vector fields f and g at u, see:
  // https://www.youtube.com/watch?v=SfOiOPuS2_U&list=PLJHszsWbB6hpk5h8lSfBkVrpjsqvUGTCx&index=24
  // ..implement both formulas: original and simplified and compare - oh  - wait - the torsion 
  // tensor does not actually depend on the vector field, only on the connection (...which is the
  // way in which we define the Christoffel symbols - right?)
  // getLieBracket


  /** Computes the covariant derivative of a vector field using Christoffel symbols. 
  Todo: make the compuation based on derivatives of basis vectors - this avoids computing the 
  contravariant metric tensor and seems generally simpler...  **/
  Mat getCovariantDerivative(const Vec& u, const FuncVecToVec& f) const
  {
    rsAssert((int)u.size() == N, "Input vector u has wrong dimensionality");

    // Algorithm: 
    //  (1) compute contravariant output vector A at u
    //  (2) compute matrix dA of partial derivatives of the vector field
    //  (3) compute rank-3 tensor of Christoffel symbols of 2nd kind at u
    //  (4) combine them via Ref.(1), Eq. 362

    Vec  A(N); f(u, A);                        // (1)
    Mat  dA = getVectorFieldDerivative(u, f);  // (2)
    Tens C  = getChristoffelSymbols2ndKind(u); // (3)

    // (4) - apply Ref.(1), Eq. 362:
    Mat D(N, N);
    D.setToZero();
    int i, j, k;
    // i: index of covariant derivative (cov. der. with respect to i-th coordinate)
    // j: index of vector component (j-th component of cov. der.)
    // k: summation index, summation is over basis-vectors (right?)
    for(i = 0; i < N; i++) {              // The covariant derivative is...
      for(j = 0; j < N; j++) {
        D(i, j) = dA(i, j);               // ...the regular partial derivative...
        for(k = 0; k < N; k++) {
          D(i, j) += C(j, k, i) * A[k];   // ...plus a sum of Christoffel-symbol terms.
        }
      }
    }
    return D;
  }




  // The code below is under construction and has not yet been tested:




  /** Under construction... totally unverified

  Absolute differentiation of a vector field with respect to a parameter t, given a t-parametrized
  curve C(t). It's evaluated at a given parametr value t, which in turn determines the position u 
  where we are in our space...
  
  ...so, is this the change of the vector field when we move a tiny bit along the curve?
  */
  Vec getAbsoluteDerivative(T t, const FuncVecToVec& A, const FuncSclToVec& C) const
  {
    // Compute position vector u at given parameter t:
    Vec u(N); C(t, u); 

    // Compute tangent to curve at u:
    Vec up(N), um(N); 
    C(t+h, up); C(t-h, um);
    Vec ut = (1/(2*h)) * (up - um);  // ut is tangent vector

    // Compute covariant derivative of vector field A at u:
    Mat dA = getCovariantDerivative(u, A);

    // Compute inner product of covariant derivative dA with tangent ut - this is defined as the 
    // absolute derivative:
    Vec r(N);
    for(int i = 0; i < N; i++)
    {
      r[i] = 0;
      for(int j = 0; j < N; j++)
        r[i] += A(i,j) * ut(j);   // verify this!
    }

    return r;
  }




  /** Under construction... not yet tested */
  Tens getRiemannTensor1stKind(const Vec& u) const
  {
    int i, j, k, l, r;
    Vec up(u), um(u);         // wiggled u coordinate vectors
    Tens cp, cm;              // Christoffel symbols at up and um

    // derivatives of Christoffel symbols of 1st kind - maybe factor out - and maybe instead of 
    // using a vector of tensors, use a rank-4 tensor - maybe we need a facility to insert a 
    // lower-rank tensor at a given (multi) index in a higher rank tensor - like 
    // chrisDeriv.insert(currentChristoffelDerivative, i); ..we want to avoid memory allocations
    std::vector<Tens> dc(N);  
    T s = 1/(2*h);
    for(i = 0; i < N; i++)
    {
      up[i] = u[i] + h;
      um[i] = u[i] - h;

      cp = getChristoffelSymbols1stKind(up);
      cm = getChristoffelSymbols1stKind(um);

      dc[i] = s * (cp - cm);             // central difference approximation
      // rsMultiArray needs to define the operators (subtraction and scalar multiplication)

      up[i] = u[i];
      um[i] = u[i];
    }


    Tens c1 = getChristoffelSymbols1stKind(u);
    Tens c2 = getChristoffelSymbols2ndKind(u);
    Tens R({N,N,N,N});
    //R.setToZero();
    for(i = 0; i < N; i++)
      for(j = 0; j < N; j++)
        for(k = 0; k < N; k++)
          for(l = 0; l < N; l++)
          {
            // (1), Eq. 558:
            R(i,j,k,l) = dc[k](j,l,i) - dc[l](j,k,i);
            for(r = 0; r < N; r++)
              R(i,j,k,l) += c1(i,l,r)*c2(r,j,k) - c1(i,k,r)*c2(r,j,l);
          }
    return R;
  }

  // 2nd kind: use Eq 560....

  Tens getRiemannTensor2ndKind(const Vec& u) const
  {
    int i, j, k, l, r;

    // compute Christoffel symbols of 2nd kind:
    Tens c = getChristoffelSymbols2ndKind(u);

    // compute derivatives of Christoffel symbols of 2nd kind
    std::vector<Tens> dc(N);  
    Vec up(u), um(u);         // wiggled u coordinate vectors
    Tens cp, cm;              // Christoffel symbols at up and um
    T s = 1/(2*h);
    for(i = 0; i < N; i++)
    {
      up[i] = u[i] + h;
      um[i] = u[i] - h;

      cp = getChristoffelSymbols2ndKind(up);
      cm = getChristoffelSymbols2ndKind(um);
      // = assignment operator is not yet implemented...

      dc[i] = s * (cp - cm);             // central difference approximation
      // rsMultiArray needs to define the operators (subtraction and scalar multiplication)

      up[i] = u[i];
      um[i] = u[i];
    }

    //double test = dc[1](0,0,0);

    // compute Riemann-Christoffel curvature tensor via (1) Eq. 560:
    Tens R({N,N,N,N});
    //R.setToZero();
    for(i = 0; i < N; i++)
      for(j = 0; j < N; j++)
        for(k = 0; k < N; k++)
          for(l = 0; l < N; l++)
          {
            // (1), Eq. 560:
            //double test1 = dc[k](i,j,l);
            //double test2 = dc[l](i,j,k);

            R(i,j,k,l) = dc[k](i,j,l) - dc[l](i,j,k);
            for(r = 0; r < N; r++)
              R(i,j,k,l) += c(r,j,l)*c(i,r,k) - c(r,j,k)*c(i,r,l);
            //int dummy = 0;
          }
    return R;
  }

  // todo: verify Bianchi identities in test code

  /** The Ricci Curvature tensor of the first kind is obtained by contracting over the first and 
  last index of the Riemann-Christoffel curvature tensor. */
  Mat getRicciTensor1stKind(const Vec& u) const
  {
    Mat  r(N, N); r.setToZero();                     // Ricci tensor (rank 2)
    Tens R = getRiemannTensor2ndKind(u);  // has rank 4, type (1,3)
    for(int i = 0; i < N; i++)
      for(int j = 0; j < N; j++)
        for(int a = 0; a < N; a++)
          r(i,j) += R(a,i,j,a);  // (1), Eq. 589
          // todo: implement a general contraction function in rsMultiArray or rsTensor..maybe
    return r;
  }

  /** The Ricci curvature tensor of the second kind is obtained by raising the first index of the
  Ricci curvature tensor of the first kind. */
  Mat getRicciTensor2ndKind(const Vec& u) const
  {
    Mat R = getRicciTensor1stKind(u);
    return raiseFirstMatrixIndex(R, u);  // (1), Eq 593
    //rsError("Not yet complete");
    //return R;
  }
  // needs test

  /** The Ricci scalar is obtained by contracting over the two indices of the Ricci tensor of the 
  second kind, i.e. by taking the trace of the matrix. */
  T getRicciScalar(const Vec& u) const
  {
    Mat R = getRicciTensor2ndKind(u);
    return R.getTrace();  // todo
  }

  // the Ricci tensor can also be computed from the Christoffel-symbols (aufstieg zu den 
  // einstein-gleichungen, eq 20.11)...but there are also derivatives of the Christoffel symbols
  // involved, so the advantage is questionable


  //Mat getEinsteinTensor(const Vec& u) const { }
  //Tens getWeylTensor(const Vec& u) const { }

  // Einstein tensor, Weyl tensor (and more):
  // https://en.wikipedia.org/wiki/List_of_formulas_in_Riemannian_geometry

  // https://en.wikipedia.org/wiki/Riemann_curvature_tensor
  // https://en.wikipedia.org/wiki/Ricci_decomposition


  // todo: scalar invariants (pg 157), infinitesimal strain (Eq. 597) - formula seems equivalent to
  // rate of strain (Eq 609) - it's the symmetric component of the gradient of a vector field, 
  // stress (Eq. 599), vorticity (Eq. 612 - anti-symmetric part of gradient)...is the "gradient" 
  // here to be understood as covariant derivative? ...probably...




  /** For two vectors given as 1xN or Nx1 matrices, this function returns whether the two vectors
  are of different variance type, i.e one covariant and the other contravariant (the variance type
  of a vector is identified by it being a row-vector (covariant) or column-vector (contravariant) ) */
  /*
  bool haveVectorsSameVariance(const rsMatrix<T>& a, const rsMatrix<T>& b)
  {
    rsAssert(a.isVector() && b.isVector());
    return (a.isRowVector() && b.isColumnVector) 
      ||   (b.isRowVector() && a.isColumnVector);
  }
  */


  /*
  T dotProduct(const rsMatrix<T>& a, const rsMatrix<T>& b)
  {
    // if one of a,b is covariant and the other is contravariant, we can do without the metric 
    // tensor

    if(haveVectorsSameVariance)
    {
      // We need the metric tensor in one or the other form
      if(a.isRowVector())
      {
        // both are row-vectors (covectors), so we need the contravariant version of the metric


      }
      else  // b is the row-vector
      {
        // both are regular column-vectors, so we need the covariant version of the metric

      }

    }
    else
    {
      // We don't need the metric tensor and can directly form the sum of the products

    }


  }
  // todo: provide implementation that takes the metric as parameter so we don't have to recompute
  // it each time
  */

  // todo: lowerVectorIndex, raiseCovectorIndex (Eq 228,229)
  // raiseIndex, lowerIndex (should apply to arbitrary tensors, Eq 230,231)

  // todo: getContravariantBasis - maybe this should be done using the covariant basis and the 
  // metric? or directly using the definition (Eq. 45 or 85) as gradients of the coordinate functions? or both and
  // then be compared? also, should the contravariant basis vectors be stored as columns of the 
  // matrix? According to Eq. 45, i think, we need to define two scalar functions u1(x,y,z) and
  // u2(x,y,z) and the first contravariant basis vector is the gradient of u1 and the 2nd the 
  // gradient of u2

  // getScaleFactors Eq. 53 69

  // getJacobian

  /*
  void computeMetric(const std::vector<T>& u, rsMatrix<T>& g)
  {
    rsAssert((int)u.size() == N);
    //rsAssert(g.hasSameShapeAs(N, N));

    // todo: convert u to basis vectors - for spherical coordinates (longitude, latitute) we would 
    // get 2 basis-vectors in 3D space - the metric tensor would be the matrix product of the 
    // matrix of these two basis vectors with the same matrix transposed
  }
  // computes covariant metric tensor at given coordinate position u = (u1, u2,..., uN)
  //
  */



  //-----------------------------------------------------------------------------------------------
  // \name Tests. After configuring the object with the various conversion functions, you can run 
  // a couple of sanity checks in order to catch errors in the client code, like passing wrong
  // or numerically too imprecise formulas.

  /** Computes the error in the roundtrip converting from curvilinear coordinates to cartesian 
  and back for the given vector u. Your functions passed to setCurvilinearToCartesian and 
  setCartesianToCurvilinear should be inverses of each other - if they in fact are not, you may 
  catch your mistake with the help of this function. */
  Vec getRoundTripError(const Vec& u) const
  {
    rsAssert((int)u.size() == N);
    Vec x(M), u2(N);
    toCartesian(  u, x);
    toCurvilinear(x, u2);
    return u2 - u;
  }

  /** Computes the matrix that results from matrix-multiplying the matrix of contravariant basis 
  vectors with the matrix of covariant basis vectors. If all is well, this should result in an 
  NxN identity matrix (aka Kronecker delta tensor) - if it doesn't, something is wrong. */
  Mat getNumericalBasisProduct(const Vec& u)
  {
    Mat cov = getCovariantBasisNumerically(u);
    Mat con = getContravariantBasisNumerically(u);
    return con * cov;
  }

  /** Same as getNumericalBasisProduct but using the analytical formulas instead (supposing that
  the user has passed some - otherwise this test doesn't make sense). */
  Mat getAnalyticalBasisProduct(const Vec& u)
  {
    rsAssert(u2xJ != nullptr, "Test makes only sense when Jacbian was assigned via setCurvToCartJacobian");
    rsAssert(x2uJ != nullptr, "Test makes only sense when Jacbian was assigned via setCartToCurvJacobian");
    Mat cov = getCovariantBasis(u);
    Mat con = getContravariantBasis(u);
    return con * cov;
  }

  /** Returns the difference between computing the Jacobian matrix (which gives the covariant basis 
  vectors) analytically and numerically, where "analytically" refers to using the function that was
  passed to setCurvToCartJacobian (if this user-defined function actually uses analytical formulas 
  or does itself something numerical internally is of course unknown to this object - but using 
  analytical formulas is the main intention of this facility).  */
  Mat getForwardJacobianError(const Vec& u)
  {
    rsAssert(u2xJ != nullptr, "Test makes only sense when Jacbian was assigned via setCurvToCartJacobian");
    Mat ana = getCovariantBasis(u);            // (supposedly) analytic formula
    Mat num = getCovariantBasisNumerically(u); // numerical approximation
    return ana - num;
  }

  /** @see getForwardJacobianError - this is the same for the inverse transformation, i.e. u(x). */
  Mat getInverseJacobianError(const Vec& u)
  {
    rsAssert(x2uJ != nullptr, "Test makes only sense when Jacbian was assigned via setCartToCurvJacobian");
    Mat ana = getContravariantBasis(u); 
    Mat num = getContravariantBasisNumerically(u);
    return ana - num;
  }



  /** Runs all tests that make sense for the given configuration using the given tolerance and 
  input vector ... */
  bool runTests(const Vec& u, T tol)
  {
    bool result = true;

    T errMax;    // maximum error
    Vec errVec;  // error vector
    Mat errMat;  // error matrix

    // roundtrip of coordinates:
    if( x2u ) { // might be not assigned
      errVec  = getRoundTripError(u);
      errMax  = rsArrayTools::maxAbs(&errVec[0], (int) errVec.size());
      result &= errMax <= tol; }

    // product of numerically computed co- and contravariant bases:
    Mat id = Mat::identity(N);
    Mat M  = getNumericalBasisProduct(u);
    errMat = M - id;
    result &= errMat.getAbsoluteMaximum() <= tol;

    // product of analytically computed co- and contravariant bases:
    if( x2uJ && u2xJ ) {
      M = getAnalyticalBasisProduct(u);
      errMat = M - id;
      result &= errMat.getAbsoluteMaximum() <= tol; }

    // difference between numerically and analytically computed covariant bases:
    if(u2xJ) {
      M = getForwardJacobianError(u);
      result &= errMat.getAbsoluteMaximum() <= tol; }

    // difference between numerically and analytically computed contravariant bases:
    if(x2uJ) {
      M = getInverseJacobianError(u);
      result &= errMat.getAbsoluteMaximum() <= tol; }

    // ...what else? ...later maybe Christoffel symbols

    return result;
  }
  // should check, if the provided formulas make sense - like x2u being inverse of u2x, the 
  // Jacobians are inverses of each other, etc. - maybe compare analytic Jacobians to numercial 
  // ones etc. - helps to catch user error - it's easy to pass an inconsistent set of equations
  // provide
  



protected:


  /** Checks, if u and x have the right dimensions. */
  void checkDims(const Vec& u, const Vec& x) const
  {
    rsAssert((int)u.size() == N);
    rsAssert((int)x.size() == M);
  }


  int N;   // dimensionality of the manifold           (see (1), pg 46 for the conventions)
  int M;   // dimensionality of the embedding space
 

  T h = 1.e-8;  
  // approximation stepsize for numerical derivatives (maybe find better default value or justify 
  // this one - maybe the default value should be the best choice, when typical coordinate values 
  // are of the order of 1)

  /** Conversions from general (contravariant) coordinates u^i to Cartesian coordinates x_i and 
  vice versa. The x_i are denoted with subscript because the embedding space is supposed to be 
  Euclidean, so we don't need a distinction between contravariant and covariant coordinates and we 
  use subscript by default. */
  FuncVecToVec u2x, x2u;
  // u2x must produce M extrinsic coordinates from N intrinsic coordinates:
  // u2x: x1(u1, u2, ..., uN)
  //      x2(u1, u2, ..., uN)
  //      ...
  //      xM(u1, u2, ..., uN)
  //
  // x2u: u1(x1, x2, ..., xM)
  //      u2(x1, x2, ..., xM)
  //      ...
  //      uN(x1, x2, ..., xM)
  // 
  // with x2u, we must be careful to only insert valid coordinates, i.e. coordinates that actually 
  // live on the manifold, otherwise the outputs may be meaningless.

  // functions for analytic Jacobians:
  FuncVecToMat u2xJ, x2uJ;

};
// todo: make it possible that the input and output dimensionalities are different - for example, 
// to specify points on the surface of a sphere, we would have two curvilinear input coordinates 
// and 3 cartesian output coordinates
// maybe have N and M as dimensionalities of the space/manifold under cosideration (e.g. the 
// sphere) and the embedding space (e.g. R^3, the 3D Euclidean space)

/*
template<class T>
void cartesianToSpherical(T x, T y, T z, T* r, T* theta, T* phi)
{
  *r     = sqrt(x*x + y*y + z*z);
  *phi   = acos(x / sqrt(x*x + y*y));        // ?= atan2(y,x)
  *theta = acos(z / sqrt(x*x + y*y + z*z));  // == acos(z / *r)
}
// see Mathematical Physics Eq. 3.12
// https://en.wikipedia.org/wiki/Spherical_coordinate_system

template<class T>
void sphericalToCartesian(T r, T theta, T phi, T* x, T* y, T* z)
{
  *x = r * sin(theta) * cos(phi);
  *y = r * sin(theta) * sin(phi);
  *z = r * cos(theta);
}
// see Mathematical Physics Eq. 3.11
*/

// see also:
// https://en.wikipedia.org/wiki/Hyperbolic_coordinates
// https://en.wikipedia.org/wiki/Elliptic_coordinate_system
// https://en.wikipedia.org/wiki/Toroidal_coordinates
// https://en.wikipedia.org/wiki/Curvilinear_coordinates

// maybe try triangular coordinates (in 3D):
// -select 3 points in 3-space: p1 = (x1,y1,z1), p2 = ..., p3
// -the general (u1,u2,u3) coordinates of a point p = (x,y,z) with respect to these points are 
//  defined as the distances of p to these 3 points:
//    u1^2 = (x-x1)^2 + (y-y1)^2 + (z-z1)^2
//    u2^2 = (x-x2)^2 + (y-y2)^2 + (z-z2)^2
//    u3^2 = (x-x3)^2 + (y-y3)^2 + (z-z3)^2
// -we may encode on which side of the plane spanned by p1,p2,p3 a point p is by applying a sign to
//  all the u-coordinates (uniformly, to treat all u values on same footing)
// -for different choices of p1,p2,p3, we obtain an infinite familiy of coordinate systems to play 
//  with
// -there are some conditions for which combinations of u-values are valid - it can't be that a 
//  point is very close to p1 and at the same time very far from p2 when p1 and p2 are somewhat 
//  close together (-> figure out details) - the bottom line is that we have to take care assigning
//  u-coordinates to make sure, the specify a valid point

// Set operations on std::vector (not yet tested):

/** Returns a set C consisting of elements that are in A and in B. */
template<class T>
std::vector<T> rsSetIntersection(const std::vector<T>& A, const std::vector<T>& B)
{
  std::vector<T> C;
  for(size_t i = 0; i < A.size(); i++)
    if(rsArrayTools::contains(&B[0], (int) B.size(), A[i]))
      C.push_back(A[i]);
  return C;
}

/** Returns a set C consisting of elements that are in A but not in B. */
template<class T>
std::vector<T> rsSetDifference(const std::vector<T>& A, const std::vector<T>& B)
{
  std::vector<T> C;
  for(size_t i = 0; i < A.size(); i++)
    if(!rsArrayTools::contains(&B[0], (int) B.size(), A[i]))
      C.push_back(A[i]);
  return C;
}

/** Returns a set C consisting of elements that are in A or in B. */
template<class T>
std::vector<T> rsSetUnion(const std::vector<T>& A, const std::vector<T>& B)
{
  std::vector<T> C = A;
  for(size_t i = 0; i < B.size(); i++)
    if(!rsArrayTools::contains(&C[0], (int) C.size(), B[i]))
      C.push_back(B[i]);
  return C;
}

// -maybe make a class rsSet with operators + for union, - for difference, * for intersection.
// -maybe keep the elements sorted - that reduces the complexity of "contains" from N to log(N)
//  -union would be some sort of merge of sorted arrays (similar as in merge sort?..but avoiding
//   duplicates)
//  -but that requires an element type that defines an order...maybe we should have both: sorted 
//   and unsorted sets

/** Removes duplicate elements from a vector A and returns the result - that's useful for turning 
arbitrary vectors inot sets (which contain each element just once). */
template<class T>
std::vector<T> rsRemoveDuplicates(const std::vector<T>& A)
{
  std::vector<T> B;
  for(size_t i = 0; i < A.size(); i++)
    if(!rsArrayTools::contains(&B[0], (int) B.size(), A[i]))
      B.push_back(A[i]);
  return B;
}


/** A class for representing sets. To optimize operations, they are kept sorted internally which 
means the data type T must support "less-than" comparisons. */

template<class T>
class rsSortedSet
{

public:


  rsSortedSet() {}

  rsSortedSet(const std::vector<T>& setData) : data(setData)
  { rsAssert(isValid(data)); }

  /** Returns true, iff the given vector is a valid representation of a sorted set. For this, it 
  must be ascendingly sorted and each element may occur only once. */
  static bool isValid(const std::vector<T>& A)
  {
    if(A.empty()) return true;  // the empty set is a valid set
    using AT = rsArrayTools;
    bool sorted = AT::isSortedAscending(&A[0], (int) A.size()); // use isSortedStrictlyAscending to disallow duplicates
    return sorted;
  }

  /** An element is in the intersection set of A and B if it is in A or in B. */
  static std::vector<T> unionSet(const std::vector<T>& A, const std::vector<T>& B)
  {
    size_t Na = A.size(), Nb = B.size();
    size_t ia = 0, ib = 0; // indices into A and B, maybe use i,j
    std::vector<T> C;
    C.reserve(Na+Nb);
    while(ia < Na && ib < Nb) {
      if(     B[ib] < A[ia]) { C.push_back(B[ib]); ib++;       }   // A[ia] >  B[ib]
      else if(A[ia] < B[ib]) { C.push_back(A[ia]); ia++;       }   // A[ia] <  B[ib]
      else                   { C.push_back(A[ia]); ia++; ib++; }}  // A[ia] == B[ib]
    while(ia < A.size()) { C.push_back(A[ia]); ia++; }
    while(ib < B.size()) { C.push_back(B[ib]); ib++; }
    return C;
  }
  // maybe rename to unionSet, etc. to avoid confusion with setters

  /** An element is in the intersection set of A and B if it is in A and in B. */
  static std::vector<T> intersectionSet(const std::vector<T>& A, const std::vector<T>& B)
  {
    size_t Na = A.size(), Nb = B.size();
    size_t ia = 0, ib = 0;
    std::vector<T> C;
    C.reserve(rsMin(Na, Nb));
    while(ia < Na && ib < Nb) {
      while(ia < Na && ib < Nb && A[ia] <  B[ib])   ia++;   // is ib < B.size() needed?
      while(ia < Na && ib < Nb && B[ib] <  A[ia])   ib++;   // is ia < A.size() needed?
      while(ia < Na && ib < Nb && B[ib] == A[ia]) { C.push_back(A[ia]); ia++; ib++; }}
    return C;
  }

  /** An element is in the difference set A "without" B if it is in A but not in B. */
  static std::vector<T> differenceSet(const std::vector<T>& A, const std::vector<T>& B)
  {
    size_t Na = A.size(), Nb = B.size();
    size_t ia = 0, ib = 0;
    std::vector<T> C;
    C.reserve(Na);
    while(ia < Na && ib < B.size()) {
      while(ia < Na && ib < Nb && A[ia] <  B[ib]) { C.push_back(A[ia]); ia++; }  // is ib < Nb needed?
      while(ia < Na && ib < Nb && A[ia] == B[ib]) { ia++; ib++;               }
      while(ia < Na && ib < Nb && B[ib] <  A[ia]) { ib++;                     }} // is ia < Na needed?
    while(ia < Na) { C.push_back(A[ia]); ia++; } 
    return C;
  }
  // while(ia < Na && ib < Na)
  //   add all elements from A that are less than our current element in B
  //   skip all elements in A and B that are equal
  //   skip all elements in B that are less than our current element in A
  // endwhile

  /** An element is in the intersection set of A and B if it is in A or in B but not in both, so 
  it's like the union but with the exclusive instead of the inclusive or. The symmetric difference
  is the union minus the intersection. */
  static std::vector<T> symmetricDifferenceSet(const std::vector<T>& A, const std::vector<T>& B)
  {
    size_t Na = A.size(), Nb = B.size();
    size_t ia = 0, ib = 0;
    std::vector<T> C;
    C.reserve(Na+Nb);
    while(ia < Na && ib < Nb) {
      while(ia < Na && ib < Nb && A[ia] <  B[ib]) { C.push_back(A[ia]); ia++; }   // is ib < B.size() needed?
      while(ia < Na && ib < Nb && B[ib] <  A[ia]) { C.push_back(B[ib]); ib++; }   // is ia < A.size() needed?
      while(ia < Na && ib < Nb && B[ib] == A[ia]) { ia++; ib++;               }}
    while(ia < A.size()) { C.push_back(A[ia]); ia++; }
    while(ib < B.size()) { C.push_back(B[ib]); ib++; }
    return C;
  }
  // needs more tests

  static std::vector<std::pair<T,T>> cartesianProduct(
    const std::vector<T>& A, const std::vector<T>& B)
  {
    size_t Na = A.size(), Nb = B.size();
    std::vector<std::pair<T,T>> C(Na * Nb);
    for(size_t ia = 0; ia < Na; ia++)
      for(size_t ib = 0; ib < Nb; ib++)
        C[ia*Nb+ib] = std::pair(A[ia], B[ib]);
    return C;
  }
  // -maybe allow different types for the elements of A and B
  // -maybe it should return a set

  // todo: implement int find(const T& x) - searches for element x via binary search and returns 
  // index, bool contains(const T& x), bool insert(const T& x), bool remove(const T& x) - return
  // value informs, if something was done

  // implement symmetric difference:
  // https://en.wikipedia.org/wiki/Set_(mathematics)#Basic_operations
  // https://en.wikipedia.org/wiki/Symmetric_difference
  // i think, it can be done with the same algo as for the intersection, just that we push in 
  // branches 1,2 and skip in branch 3, see also
  // https://www.geeksforgeeks.org/set-operations/

  // -implement function areDisjoint(A, B) ..or maybe as A.isDisjointTo(B),
  //  also: A.isSubsetOf(B), A.isSupersetOf(B) 
  // -maybe use < and <= operators where < should denote a strict subset. when A is a subset of B 
  //  and B has larger size than A, then A is a strict subset of B
  // -maybe also implement power set - but that scales with 2^N and so it gets impractical very 
  //  quickly. 
  // -maybe that could use a getAllSubsetsOfSize(int) function which may also be useful by itself
  // -maybe < should not be used for subsets because we may need some other notion of < that 
  //  satisfies the tritochomy condition, because if we want to make sets of sets with the 
  //  *sorted* set data structure, we need the comparison operator to work in a way that lest us 
  //  uniquely order the sets https://de.wikipedia.org/wiki/Trichotomie - on the other hand, we 
  //  could use a user-provided less-function for the comparisons, similar to rsBinaryHeap - that 
  //  would free the < operator again...but no - i think, it would be confusing to have a < 
  //  operator that doesn't satisfy trichotomy
  // -maybe the operators should be implemented as functions A.intersectWith(B), etc.

  // maybe implement relations as subsets of the cartesian product - then we may inquire if a 
  // particular tuple of elements is in a given relation - this can also be determined quickly by 
  // two successive binary searches...maybe we can do interesting computaions with relations, too


  const std::vector<T>& getData() const { return data; }

  /** Addition operator implements set union. */
  rsSortedSet<T> operator+(const rsSortedSet<T>& B) const
  { return rsSortedSet<T>(unionSet(this->data, B.data)); }

  /** Subtraction operator implements set difference. */
  rsSortedSet<T> operator-(const rsSortedSet<T>& B) const
  { return rsSortedSet<T>(differenceSet(this->data, B.data)); }

  /** Multiplication operator implements set intersection. */
  rsSortedSet<T> operator*(const rsSortedSet<T>& B) const
  { return rsSortedSet<T>(intersectionSet(this->data, B.data)); }

  /** Division operator implements set symmetric difference. */
  rsSortedSet<T> operator/(const rsSortedSet<T>& B) const
  { return rsSortedSet<T>(symmetricDifferenceSet(this->data, B.data)); }


  bool operator==(const rsSortedSet<T>& B) const
  { return this->data == B.data; }


protected:

  std::vector<T> data;

};
// goal: all basic set operations like union, intersection, difference, etc. should be O(N), 
// finding an element O(log(N))
// how would we deal with sets of complex numbers? they do not have a < operation. should we 
// implement a subclass that provides such an operator, like rsComplexOrderedReIm

// see also:
// https://www.geeksforgeeks.org/print-all-possible-combinations-of-r-elements-in-a-given-array-of-size-n/
// https://www.geeksforgeeks.org/heaps-algorithm-for-generating-permutations/
// https://en.cppreference.com/w/cpp/algorithm/next_permutation#Possible_implementation
// https://en.wikipedia.org/wiki/Heap%27s_algorithm#:~:text=Heap's%20algorithm
// https://en.wikipedia.org/wiki/Steinhaus%E2%80%93Johnson%E2%80%93Trotter_algorithm

// https://www.jmlr.org/papers/volume18/17-468/17-468.pdf
// Automatic Differentiation in Machine Learning: a Survey

// https://github.com/coin-or/ADOL-C

//=================================================================================================

/** Implements a datatype suitable for automatic differentiation or AD for short. In AD, a number 
is seen as the result of a function evaluation and in addition to the actual output value of the 
function, the value of the derivative is also computed and carried along through all subsequent 
operations and function applications. Derivative values can be useful in algorithms for numerical 
optimization (e.g. gradient descent), iterative nonlinear equation solvers (e.g. Newton iteration),
ordinary and partial differential equations solvers, differential geometry etc. In general, 
derivatives can be calculated by various means: 

  (1) Analytically, by directly implementing a symbolically derived formula. This is tedious, 
      error-prone and needs human effort for each individual case (unless a symbolic math engine 
      is available, but the expressions tend to swell quickly rendering the approach inefficient).
  (2) Numerically, by using finite difference approximations. This can be computationally 
      expensive and inaccurate.
  (3) Automatically, by overloading operators and functions for a number type that is augmented by
      a derivative field. The resulting algebra of such augmented numbers makes use of the well 
      known differentiation rules and derivatives of elementary functions.

The so called dual numbers are a way to implement the 3rd option. Each dual number has one field 
"v" representing the actual (value of the) number itself and an additional field "d" for 
representing the derivative. In all arithmetic operations that we do with a pair of operands, the 
first value component "v" is computed as usual and the derivative component "d" is computed using
the well known differentiation rules: sum-rule, difference-rule, product-rule and quotient-rule. 
Likewise, in univariate function evaluations, we apply the chain-rule. 

Mathematically, we can think of the dual numbers as being constructed from the real numbers in a 
way similar to the complex numbers. For these, we postulate the existence of a number i which has
the property i^2 = -1. No real number has this property, so i can't be a real number. For the dual
numbers, we postulate a nonzero number epsilon (denoted here as E) with the property E^2 = 0. No 
real number (except zero, which was excluded) has this property, so E can't be a real number. The 
dual numbers are then numbers of the form a + b*E, just like the complex numbers are of the form 
a + b*i, where a and b are real numbers. ..i think, this has relations to nonstandard analysis with
hyperreal numbers and E can be thought of as being the infinitesimal....figure that out and 
explain more...maybe mention also also hyperbolic numbers...they do not form a field, only a ring,
because the quotient rule demands that the v-part of divisor must be nonzero - so any dual number
with zero v-part is a number that we cant divide by - and these are not the neutral element(s)
of addition (there can be only one anyway)


To implement multidimensional derivatives (gradients, Jacobians, etc.), we can use rsMatrix as 
template type T. Then, the input is an Mx1 matrix, the output is an Nx1 matrix and the Jacobian is 
an NxM matrix. ...hmm...this is not how i did it in the examples...
...what about higher derivative via nesting?

...At some point, the derivative value must be seeded...i think, this is called 
"forward mode"...We seed the derivative field with 1 by default-constructing a dual number from a 
real number...explain also what reverse mode is..tbc...

...under construction... */

template<class TVal, class TDer>
class rsDualNumber 
{

public:



  TVal v;  // function value, "real part" or "standard part"
  TDer d;  // derivative, "infinitesimal part" (my word)

  rsDualNumber(TVal value = TVal(0), TDer derivative = TDer(1)) : v(value), d(derivative) {}
  // maybe the derivative should default to 1? what is most convenient? to seed or not to seed?


  using DN = rsDualNumber<TVal, TDer>;   // shorthand for convenience



  //-----------------------------------------------------------------------------------------------
  // \name Inquiry

  TVal getValue()      const { return v; }
  TDer getDerivative() const { return d; }

  //-----------------------------------------------------------------------------------------------
  // \name Arithmetic operators

  /** Unary minus just applies minus sign to both parts. */
  DN operator-() const { return DN(-v, -d); }

  /** Implements sum rule: (f+g)' = f' + g'. */
  DN operator+(const DN& y) const { return DN(v + y.v, d + y.d); }

  /** Implements difference rule: (f-g)' = f' - g'. */
  DN operator-(const DN& y) const { return DN(v - y.v, d - y.d); }

  /** Implements product rule: (f*g)' = f' * g + g' * f. */
  DN operator*(const DN& y) const { return DN(v * y.v, d*y.v + v*y.d ); }

  /** Implements quotient rule: (f/g)' = (f' * g - g' * f) / g^2. */
  DN operator/(const DN& y) const { return DN(v / y.v, (d*y.v - v*y.d)/(y.v*y.v) ); }
    // requires that y.v is nonzero - what does this mean for the algebraic structure od dual 
    // numbers? do they form a ring? it's not a field because *any* number y for which v is zero 
    // can't be a divisor - but if the d-field of y is nonzero, then y is not 0 (the neutral 
    // element of addition)...right? ..so we have nonzero elements that we can't divide by, so we
    // have no field
    // maybe we could do something else when y.v == 0. in this case, the infinitesimal part gets 
    // blown up to infinity, so maybe it could make sense to add the numerator 
    // (d*y.v - v*y.d) = -v*y.d to the real part? ...highly speculative - maybe try it and see 
    // what happens...but actually, the 0 in the denomiantor is squared, so it gets blown up to
    // "infinity-squared" ...so maybe it should overblow the real part...oh - that's what it 
    // already does anyway...maybe it should be v - v*y.d = v*(1-y.d)...maybe consider the limits
    // when y.v goes to 0

  template<class Ty> DN operator+(const Ty& y) const { return DN(v + TVal(y), d                ); }
  template<class Ty> DN operator-(const Ty& y) const { return DN(v - TVal(y), d                ); }
  template<class Ty> DN operator*(const Ty& y) const { return DN(v * TVal(y), d * TDer(TVal(y))); }
  template<class Ty> DN operator/(const Ty& y) const { return DN(v / TVal(y), d / TDer(TVal(y))); }

  // maybe rename operands from x,y to a,b - x,y should be used for function inputs and ouputs in
  // expressions like y = f(x)

  // todo: add boilerplate for +=,-=,*=,...



  //-----------------------------------------------------------------------------------------------
  // \name Comparison operators

  bool operator==(const DN& y) const { return v == y.v && d == y.d; } // maybe we should only compare v
  bool operator!=(const DN& y) const { return !(*this == y); }
  bool operator< (const DN& y) const { return v <  y.v; }
  bool operator<=(const DN& y) const { return v <= y.v; }
  bool operator> (const DN& y) const { return v >  y.v; }
  bool operator>=(const DN& y) const { return v >= y.v; }

  // maybe we should take the d-part into account in the case of equal v-parts. this video says 
  // something about the hyperreal numbers being an *ordered* field:
  // https://www.youtube.com/watch?v=ArAjEq8uFvA
  // when we do it like this, the set of relations {<,==,>} satisfy the trichotomy law: any pair 
  // of numbers is in one and only one of those 3 relations

};

//-------------------------------------------------------------------------------------------------
// Operators for left argument of type Tx that can be converted into TVal:

template<class TVal, class TDer, class Tx>
rsDualNumber<TVal, TDer> operator+(const Tx& x, const rsDualNumber<TVal, TDer>& y)
{ return rsDualNumber<TVal, TDer>(TVal(x) + y.v, y.d); } // ok

template<class TVal, class TDer, class Tx>
rsDualNumber<TVal, TDer> operator-(const Tx& x, const rsDualNumber<TVal, TDer>& y)
{ return rsDualNumber<TVal, TDer>(TVal(x) - y.v, -y.d) ; } // ok

template<class TVal, class TDer, class Tx>
rsDualNumber<TVal, TDer> operator*(const Tx& x, const rsDualNumber<TVal, TDer>& y)
{ return rsDualNumber<TVal, TDer>(TVal(x) * y.v, TDer(TVal(x)) * y.d); } // ok

template<class TVal, class TDer, class Tx>
rsDualNumber<TVal, TDer> operator/(const Tx& x, const rsDualNumber<TVal, TDer>& y)
{ return rsDualNumber<TVal, TDer>(TVal(x) / y.v, -TDer(x)*y.d/(y.v*y.v) ); } // ok

// maybe reduce the noise via #defines here, too

//-------------------------------------------------------------------------------------------------
// Elementary functions of dual numbers. The v-parts are computed as usual and the d-parts are 
// computed via the chain rule: (f(g(x)))' = g'(x) * f'(g(x)). We use some preprocessor 
// #definitions to reduce the verbosity of the boilerplate code. They are #undef'd when we are done
// with them:

#define RS_CTD template<class TVal, class TDer>  // class template declarations
#define RS_DN  rsDualNumber<TVal, TDer>          // dual number
#define RS_PFX RS_CTD RS_DN                      // prefix for the function definitions

RS_PFX rsSin(RS_DN x) { return RS_DN(rsSin(x.v),  x.d*rsCos(x.v)); }
RS_PFX rsCos(RS_DN x) { return RS_DN(rsCos(x.v), -x.d*rsSin(x.v)); }
RS_PFX rsExp(RS_DN x) { return RS_DN(rsExp(x.v),  x.d*rsExp(x.v)); }

// not tested:
RS_PFX rsLog( RS_DN x) { return RS_DN(rsLog(x.v),  x.d/x.v); } // requires x.v > 0
RS_PFX rsSqrt(RS_DN x) { return RS_DN(rsSqrt(x.v), x.d*TVal(0.5)/sqrt(x.v)); } 
// requires x.v > 0 - todo: make it work for x.v >= 0 - the derivative part at 0 should be computed
// by using a limit


// todo: tan, cbrt, pow, abs, sinh, cosh, tanh, asin, acos, atan, atan2, etc.
// what about floor and ceil? should their derivatives be a delta-comb? well - maybe i should not
// care about them - they are not differentiable anyway

#undef RS_CTD
#undef RS_DN
#undef RS_PFX




//-------------------------------------------------------------------------------------------------
// Functions for nested dual numbers - can be used to compute 2nd derivatives:
// ...works only with simply nested dual numbers - nesting twice doesn't work - maybe nesting 
// should be postponed - it's a mess

#define RS_CTD template<class T1, class T2, class T3>  // class template declarations
#define RS_IDN rsDualNumber<T2, T3>                    // inner dual number
#define RS_ODN rsDualNumber<T1, RS_IDN>                // outer dual number

// types: x.v: T1, x.d.v: T2, x.d.d: T3

RS_CTD RS_ODN rsSin(RS_ODN x) { return RS_ODN(rsSin(x.v),  x.d.v*rsCos(RS_IDN(x.v))); }
RS_CTD RS_ODN rsCos(RS_ODN x) { return RS_ODN(rsCos(x.v), -x.d.v*rsSin(RS_IDN(x.v))); }


//RS_CTD RS_ODN rsExp(RS_ODN x) { return RS_ODN(rsExp(x.v),  x.d.v*rsExp(RS_IDN(x.v))); }
//RS_CTD RS_ODN rsExp(RS_ODN x) { return RS_ODN(rsExp(x.v),  RS_IDN(x.d.v)*rsExp(RS_IDN(x.v))); }
//RS_CTD RS_ODN rsExp(RS_ODN x) { return RS_ODN(rsExp(x.v),  RS_IDN(x.d.v,0)*rsExp(RS_IDN(x.v))); }  // 3rd wrong
//RS_CTD RS_ODN rsExp(RS_ODN x) { return RS_ODN(rsExp(x.v),  RS_IDN(x.d.v,1)*rsExp(RS_IDN(x.v))); }    // 2nd,3rd wrong
//RS_CTD RS_ODN rsExp(RS_ODN x) { return RS_ODN(rsExp(x.v),  T2(x.d.v)*rsExp(RS_IDN(x.v))); } // 3rd wrong
//RS_CTD RS_ODN rsExp(RS_ODN x) { return RS_ODN(rsExp(x.v),  RS_IDN(T2(x.d.v))*rsExp(RS_IDN(x.v))); }
//RS_CTD RS_ODN rsExp(RS_ODN x) { return RS_ODN(rsExp(x.v),  RS_IDN(T2(x.d.v),0)*rsExp(RS_IDN(x.v))); }
//RS_CTD RS_ODN rsExp(RS_ODN x) { return RS_ODN(rsExp(x.v), RS_IDN(T3(x.d.v))*rsExp(RS_IDN(x.v))); } 
//RS_CTD RS_ODN rsExp(RS_ODN x) { return RS_ODN(rsExp(x.v), T3(x.d.v)*rsExp(RS_IDN(x.v))); }
// 3rd derivative is wrong - seems to be multiplied by factor 2

RS_CTD RS_ODN rsExp(RS_ODN x) 
{ 
  T1     v = rsExp(x.v);                      // T1
  //RS_IDN d = T2(x.d.v) * rsExp(RS_IDN(x.v));  // T2 * Dual<T2,T3> -> Dual(x.d.v,1) * exp(Dual(x.v,1))

  // if T2 is itself a dual number, this does not work - both factors get constructed with a 1 for the
  // d part and in the product rule f'*g + g'*f, we end up producing twice the desired value due to 
  // adding two equal terms

  RS_IDN d = RS_IDN(x.d.v,0) * rsExp(RS_IDN(x.v));
  // hmm...this also doesn't work - we still get the extra factor of 2

  //RS_IDN d = T2(x.d.v,0) * rsExp(RS_IDN(x.v)); // no compile with T2=float

  //RS_IDN d = RS_IDN(x.d.v * rsExp(RS_IDN(x.v)));

  //RS_IDN d = x.d.v * rsExp(RS_IDN(T3(x.v)));
  //RS_IDN d = T2(x.d.v) * rsExp(RS_IDN(T2(x.v)));
  //RS_IDN d = RS_IDN(T3(x.d.v)) * rsExp(RS_IDN(T2(x.v)));
  // somewhere, there's a constructor call missing and one d-element is not seeded, i think

  return RS_ODN(v,  d); 
}

// Why does it actually work when using x.d.v as inner derivative? It probably should not be 
// surprising, but i'm not sure about why. Maybe try nesting twice - doesn't work...maybe we need
// to wrap something else into a constructor call? i tried -RS_IDN(x.d.v), RS_IDN(-x.d.v) for 
// d-part of rsCos - but that doesn't work...maybe we need to use 0 as 2nd argument?

// todo: log, tan, sqrt, pow, abs, sinh, cosh, tanh, asin, acos, atan, atan2, etc.

#undef RS_CTD
#undef RS_IDN
#undef RS_NDN

// maybe for doubly nested dual numbers, we will need yet another specialzation? ...if so, will it 
// end there or will we need another for triply nested ones and so on? that would be bad!





/*
// not yet tested:
template<class TVal, class TDer>
rsDualNumber<TVal, TDer> rsSqrt(rsDualNumber<TVal, TDer> x) 
{ return rsDualNumber<TVal, TDer>(sqrt(x.v), x.d*T(0.5)/sqrt(x.v)); }  // verify

template<class TVal, class TDer>
rsDualNumber<TVal, TDer> rsLog(rsDualNumber<TVal, TDer> x) 
{ return rsDualNumber<TVal, TDer>(log(x.v), x.d/x.v); }  // requires x.v > 0

template<class TVal, class TDer>
rsDualNumber<TVal, TDer> rsPow(rsDualNumber<TVal, TDer> x, TVal p)
{ return rsDualNumber<TVal, TDer>(pow(x.v, p), x.d*TDer(p)*pow(x.v, p-1)); }  // requires x.v != 0
// what, if p is also an dual number? ...this requires more thought....

template<class TVal, class TDer>
rsDualNumber<TVal, TDer> rsAbs(rsDualNumber<TVal, TDer> x) 
{ return rsDualNumber<TVal, TDer>(rsAbs(x.v), x.d*rsSign(x.v)); }  // requires x.v != 0..really?
*/



/** A number type for automatic differentiation in reverse mode. The operators and functions are 
implemented in a way so keep a record of the whole computation. At the end of the computation, a 
call to getDerivative() triggers the computation of the derivative by running through the whole 
computation backwards

....i don't know yet, if the implementation is on the right track - i just started without much
of a plan...

*/

template<class TVal, class TDer>
class rsAutoDiffNumber
{

//protected:

public:



  enum class OperationType
  {
    neg, add, sub, mul, div, sqrt, sin, cos, exp  // more to come
  };

  inline static const TVal NaN = RS_NAN(TVal);
  using ADN = rsAutoDiffNumber<TVal, TDer>;   // shorthand for convenience


  /** Structure to store the operations */
  struct Operation
  {
    OperationType type; // type of operation
    ADN op1;            // first operand
    ADN op2;            // second operand
    TVal res;           // result
    Operation(OperationType _type, ADN _op1, ADN _op2, TVal _res)
      : type(_type), op1(_op1), op2(_op2), res(_res) {}
  };


  std::vector<Operation>& ops;  // "tape" of recorded operations



  using OT  = OperationType;
  using OP  = Operation;



  /** Pushes an operation record consisting of a type, two operands and a result onto the operation 
  tape ops and returns the result of the operation as rsAutoDiffNumber. */
  ADN push(OperationType type, ADN op1, ADN op2, TVal res)
  {
    ops.push_back(OP(type, op1, op2, res)); 
    return ADN(res, ops);
  }

public:

  TVal v;        // value
  TDer d = NaN;  // (partial) derivative

  rsAutoDiffNumber(TVal value, std::vector<Operation>& tape) : v(value), /*a(NaN),*/ ops(tape) {}




  void initLoc() { loc = this; }
  // inits the location pointer - this is a bad API and temporary
  // get rid of this - or at least avoid having to call it from client code - client created 
  // numbers with memory locations should always automatically assign the location and temporaries
  // shouldn't ...maybe we need to pass a flag to the constructor to indicate a memory variable
  // if this is not possible, find a better name - like enable
  // i think, maybe it's possible by making sure that push() sets loc to nullptr - then the 
  // standard constructor may automatically set loc = this. ...but is it really safe to assume,
  // that all variable locations are still valid - no: the user may define their own functions 
  // using temporary variables - these will be invalid, when the function returns - maybe we 
  // indeed need manual enable calls

  //-----------------------------------------------------------------------------------------------
  // \name Inquiry

  TVal getValue()      const { return v; }


  void computeDerivatives()
  {
    TDer d(1);
    // i think, this accumulator is wrong - we whould use only the v,d fields of the operand(s) and
    // the result - so the result may also have to be an ADN...or maybe not?

    //TDer d1, d2;

    int i = (int) ops.size()-1;

    if(i < 0)
      return;

    //ops[i].res.d = TVal(1);  // init - may not always be suitable


    while(i >= 0)
    {

      //ops[i].op1d = getOpDerivative(ops[i]);

      if(ops[i].type == OT::add)
      {
        ops[i].op1.d = d;
        ops[i].op2.d = d;
      }
      else if(ops[i].type == OT::sub)
      {
        ops[i].op1.d =  d;
        ops[i].op2.d = -d;
      }
      else if(ops[i].type == OT::mul)
      {
        ops[i].op1.d = d * ops[i].op2.v;  // (x*y)_x = y
        ops[i].op2.d = d * ops[i].op1.v;  // (x*y)_y = x
      }
      else if(ops[i].type == OT::div)
      {
        ops[i].op1.d =  d                /  ops[i].op2.v;                  // (x/y)_x =  1/y
        ops[i].op2.d = -d * ops[i].op1.v / (ops[i].op2.v * ops[i].op2.v);  // (x/y)_y = -x/y^2
      }
      else
      {
        // operation is a unary function
        d *= getOpDerivative(ops[i]);
        ops[i].op1.d = d;
        ops[i].op2.d = NaN;
      }

      

      // assign derivative fields in memory variables, if applicable:
      if(ops[i].op1.loc != nullptr) ops[i].op1.loc->d = ops[i].op1.d;
      if(ops[i].op2.loc != nullptr) ops[i].op2.loc->d = ops[i].op2.d;






      i--;

    }

  }

  // rename to getFunctionDerivative and use it only for unary functions
  TDer getOpDerivative(const Operation& op) const
  {
    switch(op.type)
    {
    //case OT::add: return   op.op1.v + op.op2.v;  // is this correct?

      // for mul..do we have to call getOpDerivative twice with both operands
      // or maybe reverse mode is only good for univariate operations? they always only talk about
      // multiplying the Jacobians left-to-right or right-to-left - but here, we need to add two
      // Jacobians...maybe we have to use recursion in getDerivative
      // or maybe the Operation struct needs derivative fields for both operands? that are filled
      // in the backward pass?


    case OT::sqrt: return  TVal(0.5)/rsSqrt(op.op1.v); // maybe we could also use TVal(0.5)/op.res
    case OT::sin:  return  rsCos(op.op1.v);
    case OT::cos:  return -rsSin(op.op1.v);
    case OT::exp:  return  rsExp(op.op1.v);

      // but what about the binary operators? 

    default: return TDer(0);
    }
  }

  //-----------------------------------------------------------------------------------------------
  // \name Arithmetic operators

  ADN operator-() { return push(neg, v, NaN, -v); }

  ADN operator+(const ADN& y) { return push(OT::add, *this, y, v + y.v); }
  ADN operator-(const ADN& y) { return push(OT::sub, *this, y, v - y.v); }
  ADN operator*(const ADN& y) { return push(OT::mul, *this, y, v * y.v); }
  ADN operator/(const ADN& y) { return push(OT::div, *this, y, v / y.v); }


  ADN operator=(const ADN& y) 
  { 
    v = y.v;
    ops = y.ops;
    return *this;
  }

  // void setOperationTape(std::vector<Operation>& newTape) { ops = newTape; }


  
//protected:  // try to make protected - but that may need a lot of boilerplat friend declarations

  rsAutoDiffNumber* loc = nullptr;

};

#define RS_CTD template<class TVal, class TDer>  // class template declarations
#define RS_ADN rsAutoDiffNumber<TVal, TDer>      // 
#define RS_OP RS_ADN::OperationType
#define RS_PFX RS_CTD RS_ADN                      // prefix for the function definitions

RS_PFX rsSqrt(RS_ADN x) { return x.push(RS_OP::sqrt, x, RS_ADN(0.f, x.ops), rsSqrt(x.v)); }
RS_PFX rsSin( RS_ADN x) { return x.push(RS_OP::sin,  x, RS_ADN(0.f, x.ops), rsSin( x.v)); }
RS_PFX rsCos( RS_ADN x) { return x.push(RS_OP::cos,  x, RS_ADN(0.f, x.ops), rsCos( x.v)); }
RS_PFX rsExp( RS_ADN x) { return x.push(RS_OP::exp,  x, RS_ADN(0.f, x.ops), rsExp( x.v)); }
// the 2nd operand is a dummy - maybe define a macro for it


//RS_PFX rsSqrt(RS_ADN x) { return x.push(RS_OP::sqrt, x, RS_ADN(RS_ADN::NaN), rsSqrt(x.v)); }
//RS_PFX rsSin( RS_ADN x) { return x.push(RS_OP::sin,  x, RS_ADN(RS_ADN::NaN), rsSin( x.v)); }
//RS_PFX rsCos( RS_ADN x) { return x.push(RS_OP::cos,  x, RS_ADN(RS_ADN::NaN), rsCos( x.v)); }
//RS_PFX rsExp( RS_ADN x) { return x.push(RS_OP::exp,  x, RS_ADN(RS_ADN::NaN), rsExp( x.v)); }

#undef RS_CTD
#undef RS_DN
#undef RS_OP
#undef RS_PFX




// https://en.wikipedia.org/wiki/Automatic_differentiation
// https://en.wikipedia.org/wiki/Automatic_differentiation#Automatic_differentiation_using_dual_numbers
// https://www.neidinger.net/SIAMRev74362.pdf
// https://en.wikipedia.org/wiki/Dual_number

// http://www.autodiff.org/?module=Introduction&submenu=Surveys

// https://www.youtube.com/watch?v=xtZ0_0DP_GI Automatic differentiation using ForwardDiff.jl and ReverseDiff.jl (Jarrett Revels, MIT)
// http://www.juliadiff.org/ForwardDiff.jl/stable/
// https://github.com/JuliaDiff/ForwardDiff.jl
// https://github.com/JuliaDiff

// https://en.wikipedia.org/wiki/Grassmann_number
// https://en.wikipedia.org/wiki/Split-complex_number

// what about multivariate functions and partial derivatives?

template<class T>
class rsDualComplexNumber  // "duco"
{

public:

  T a, b, c, d;

  using DCN = rsDualComplexNumber<T>;


  rsDualComplexNumber(T real = T(0), T imag = T(0), T dual = T(0), T imagDual = T(0)) 
    : a(real), b(imag), c(dual), d(imagDual) {}




  DCN operator+(const DCN& z) const { return DCN(a + z.a, b + z.b, c + z.c, d + z.d); }
  DCN operator-(const DCN& z) const { return DCN(a - z.a, b - z.b, c - z.c, d - z.d); }
  DCN operator*(const DCN& z) const
  {
    return DCN(a*z.a - b*z.b, 
               a*z.b + z.a*b, 
               a*z.c + z.a*c - b*z.d - z.b*d, 
               a*z.d + z.a*d + b*z.c + z.b*c);
  }

  DCN operator/(const DCN& z) const
  {
    T A = z.a*z.a + z.b*z.b;
    T D = T(2)*(z.a*z.d - z.b*z.c);

    DCN tmp = *this * DCN(z.a, -z.b, -z.c, z.d);  // product after first augmentation
    tmp = tmp * DCN(A, T(0), T(0), -D);        // product after second augmentation
    T s = T(1) / (A*A);
    tmp.a *= s;
    tmp.b *= s;
    tmp.c *= s;
    tmp.d *= s;

    return tmp;
  }
  // todo: simplify, optimize - the second multiplication contains lots of zeros, the scaling
  // should use an operator / that takes a 2nd parameter of type T

  /*
  DCN operator/(const T& z) const { }
  */


};

#define RS_CTD template<class T> 
#define RS_DCN rsDualComplexNumber<T> 
#define RS_CMP std::complex<T> 
#define RS_PFX RS_CTD RS_DCN 

RS_PFX rsSin(RS_DCN x)
{
  // extract complex value and derivative of inputs:
  RS_CMP v(x.a, x.b);
  RS_CMP d(x.c, x.d);

  // compute derivative and value of output:
  d *= cos(v);    // chain rule
  v  = sin(v);    // regular function application

  // construct DCN from the 2 complex numbers:
  return RS_DCN(v.real(), v.imag(), d.real(), d.imag());
}
// needs verification

#undef RS_CTD
#undef RS_DCN
#undef RS_CMP 
#undef RS_PFX

// is this a restricted case of this?: https://en.wikipedia.org/wiki/Dual_quaternion
// oh - there are already dual complex numbers - but they work differently:
// https://en.wikipedia.org/wiki/Dual-complex_number
// ...so we should use another name - how about DiffComplex or DiComplex - when they are useful for
// automatic differentiation in the complex domain - we'll see
// todo: define elementary functions exp, sin, cos, sqrt
// implement operators that allow mixed operations with std::complex


/*
creating movies from pictures:
https://askubuntu.com/questions/971119/convert-a-sequence-of-ppm-images-to-avi-video
https://superuser.com/questions/624567/how-to-create-a-video-from-images-using-ffmpeg


https://hamelot.io/visualization/using-ffmpeg-to-convert-a-set-of-images-into-a-video/


not good:
ffmpeg -r 60 -f image2 -s 1920x1080 -i pic%04d.png -vcodec libx264 -crf 25  -pix_fmt yuv420p test.mp4
ffmpeg -r 25 -f image2 -s 300x300 -i Epidemic_Frame%d.ppm -vcodec libx264 -crf 0 test.mp4
ffmpeg -r 25 -f image2 -s 300x300 -i Epidemic_Frame%d.ppm -vcodec libx264 -crf 0  -pix_fmt rgb24 test.mp4
ffmpeg -r 25 -f image2 -s 100x100 -i Epidemic_Frame%d.ppm -vcodec libx264 -crf 0  -pix_fmt rgb24 test.mp4
  this works with the 100x100 images in the "Box" folder - it seems to be related to the background 
  gradients? with uniform background color, all seems well


copy ffmpeg.exe into the folder where the images reside, open a cmd window in that folder and 
enter:

good:
ffmpeg -r 25 -f image2 -s 100x100 -i Epidemic_Frame%d.ppm -vcodec libx264 -crf 10  -pix_fmt yuv420p SIRP.mp4

ffmpeg -r 25 -i VideoTempFrame%d.ppm -vcodec libx264 -crf 10 -pix_fmt yuv420p


path:
C:\Program Files\ffmpeg\bin

https://www.java.com/en/download/help/path.xml


-Righ-click start
-System
-Advanced
-Environment Variables
-select PATH
-click Edit
-append the new path, sperated by a semicolon from what is already there
-confirm with OK and restart the computer
...i don't know, if it's sufficient to add it to User Settings, PATH, and/or if it has to be added
// to system settings -> Path - i added it to both but that may not be necessarry -> figure out


// i get a video file, but it's garbage whe using -pix_fmt rgb24 (sort of - it's still somewhat 
// recognizable, though)...but -pix_fmt yuv420p seems to work

// https://stackoverflow.com/questions/45929024/ffmpeg-rgb-lossless-conversion-video

ffmpeg gui frontends:
https://www.videohelp.com/software/Avanti
https://www.videohelp.com/software/MediaCoder
https://sourceforge.net/projects/ffmpegyag/

// here is a list:
https://github.com/amiaopensource/ffmpeg-amia-wiki/wiki/3)-Graphical-User-Interface-Applications-using-FFmpeg
http://qwinff.github.io/
http://www.avanti.arrozcru.org/
https://handbrake.fr/
http://www.squared5.com/
https://sourceforge.net/projects/ffmpegyag/


*/