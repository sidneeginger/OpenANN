#include <OpenANN/Net.h>
#include <OpenANN/layers/Input.h>
#include <OpenANN/layers/AlphaBetaFilter.h>
#include <OpenANN/layers/FullyConnected.h>
#include <OpenANN/layers/Compressed.h>
#include <OpenANN/layers/Extreme.h>
#include <OpenANN/layers/Convolutional.h>
#include <OpenANN/layers/Subsampling.h>
#include <OpenANN/layers/MaxPooling.h>
#include <OpenANN/layers/LocalResponseNormalization.h>
#include <OpenANN/layers/Dropout.h>
#include <OpenANN/RBM.h>
#include <OpenANN/io/DirectStorageDataSet.h>
#include <OpenANN/optimization/IPOPCMAES.h>
#include <OpenANN/optimization/LMA.h>
#include <OpenANN/optimization/MBSGD.h>

namespace OpenANN {

Net::Net()
  : dataSet(0), testDataSet(0), deleteDataSet(false), deleteTestSet(false),
    errorFunction(SSE), dropout(false), initialized(false), N(0), L(0)
{
  layers.reserve(3);
  infos.reserve(3);
}

Net::~Net()
{
  if(deleteDataSet)
  {
    delete dataSet;
    dataSet = 0;
  }
  if(deleteTestSet)
  {
    delete testDataSet;
    testDataSet = 0;
  }
  for(int i = 0; i < layers.size(); i++)
  {
    delete layers[i];
    layers[i] = 0;
  }
  layers.clear();
}

Net& Net::inputLayer(int dim1, int dim2, int dim3)
{
  return addLayer(new Input(dim1, dim2, dim3));
}

Net& Net::alphaBetaFilterLayer(double deltaT, double stdDev)
{
  return addLayer(new AlphaBetaFilter(infos.back(), deltaT, stdDev));
}

Net& Net::fullyConnectedLayer(int units, ActivationFunction act, double stdDev,
                              bool bias, double maxSquaredWeightNorm)
{
  return addLayer(new FullyConnected(infos.back(), units, bias, act, stdDev,
                                     maxSquaredWeightNorm));
}

Net& Net::restrictedBoltzmannMachineLayer(int H, int cdN, double stdDev,
                                          bool backprop)
{
  return addLayer(new RBM(infos.back().outputs(), H, cdN, stdDev, backprop));
}

Net& Net::compressedLayer(int units, int params, ActivationFunction act,
                          const std::string& compression, double stdDev,
                          bool bias)
{
  return addLayer(new Compressed(infos.back(), units, params, bias, act,
                                compression, stdDev));
}

Net& Net::extremeLayer(int units, ActivationFunction act, double stdDev,
                       bool bias)
{
  return addLayer(new Extreme(infos.back(), units, bias, act, stdDev));
}

Net& Net::convolutionalLayer(int featureMaps, int kernelRows, int kernelCols,
                             ActivationFunction act, double stdDev, bool bias)
{
  return addLayer(new Convolutional(infos.back(), featureMaps, kernelRows,
                                   kernelCols, bias, act, stdDev));
}

Net& Net::subsamplingLayer(int kernelRows, int kernelCols,
                           ActivationFunction act, double stdDev, bool bias)
{
  return addLayer(new Subsampling(infos.back(), kernelRows, kernelCols, bias, act, stdDev));
}

Net& Net::maxPoolingLayer(int kernelRows, int kernelCols)
{
  return addLayer(new MaxPooling(infos.back(), kernelRows, kernelCols));
}

Net& Net::localReponseNormalizationLayer(double k, int n, double alpha,
                                         double beta)
{
  return addLayer(new LocalResponseNormalization(infos.back(), k, n, alpha,
                                                 beta));
}

Net& Net::dropoutLayer(double dropoutProbability)
{
  return addLayer(new Dropout(infos.back(), dropoutProbability));
}

Net& Net::addLayer(Layer* layer)
{
    OPENANN_CHECK(layer != 0);

    OutputInfo info = layer->initialize(parameters, derivatives);
    layers.push_back(layer);
    infos.push_back(info);
    L++;
    return *this;
}

Net& Net::addOutputLayer(Layer* layer)
{
    addLayer(layer);
    initializeNetwork();
    return *this;
}


Net& Net::outputLayer(int units, ActivationFunction act, double stdDev, bool bias)
{
  fullyConnectedLayer(units, act, stdDev, bias);
  initializeNetwork();
  return *this;
}

Net& Net::compressedOutputLayer(int units, int params, ActivationFunction act,
                                const std::string& compression, double stdDev,
                                bool bias)
{
  compressedLayer(units, params, act, compression, stdDev, bias);
  initializeNetwork();
  return *this;
}

unsigned int Net::numberOflayers()
{
  return L;
}

Layer& Net::getLayer(unsigned int l)
{
  OPENANN_CHECK(l >= 0 && l < L);
  return *layers[l];
}

OutputInfo Net::getOutputInfo(unsigned int l)
{
  OPENANN_CHECK(l >= 0 && l < L);
  return infos[l];
}

void Net::initializeNetwork()
{
  P = parameters.size();
  tempInput.resize(1, infos[0].outputs());
  tempOutput.resize(1, infos.back().outputs());
  tempError.resize(1, infos.back().outputs());
  tempGradient.resize(P);
  parameterVector.resize(P);
  for(int p = 0; p < P; p++)
    parameterVector(p) = *parameters[p];
  initialized = true;
}

Net& Net::useDropout(bool activate)
{
  dropout = activate;
}

Learner& Net::trainingSet(Eigen::MatrixXd& trainingInput, Eigen::MatrixXd& trainingOutput)
{
  dataSet = new DirectStorageDataSet(&trainingInput, &trainingOutput);
  deleteDataSet = true;
  N = dataSet->samples();
  return *this;
}

Learner& Net::trainingSet(DataSet& trainingSet)
{
  if(deleteDataSet)
    delete dataSet;
  dataSet = &trainingSet;
  deleteDataSet = false;
  N = dataSet->samples();
  return *this;
}

Net& Net::testSet(Eigen::MatrixXd& testInput, Eigen::MatrixXd& testOutput)
{
  if(deleteTestSet)
    delete testDataSet;
  testDataSet = new DirectStorageDataSet(&testInput, &testOutput);
  deleteTestSet = true;
  return *this;
}

Net& Net::testSet(DataSet& testSet)
{
  testDataSet = &testSet;
  deleteTestSet = false;
  return *this;
}

Net& Net::setErrorFunction(ErrorFunction errorFunction)
{
  this->errorFunction = errorFunction;
}

void Net::finishedIteration()
{
  bool dropout = this->dropout;
  this->dropout = false;
  if(dataSet)
    dataSet->finishIteration(*this);
  if(testDataSet)
    testDataSet->finishIteration(*this);
  this->dropout = dropout;
}

Eigen::VectorXd Net::operator()(const Eigen::VectorXd& x)
{
  tempInput = x.transpose();
  Eigen::MatrixXd* y = &tempInput;
  for(std::vector<Layer*>::iterator layer = layers.begin();
      layer != layers.end(); layer++)
    (**layer).forwardPropagate(y, y, dropout);
  tempOutput = *y;
  if(errorFunction == CE)
    OpenANN::softmax(tempOutput);
  return tempOutput.transpose();
}

unsigned int Net::dimension()
{
  return P;
}

unsigned int Net::examples()
{
  return N;
}

Eigen::VectorXd Net::currentParameters()
{
  return parameterVector;
}

void Net::setParameters(const Eigen::VectorXd& parameters)
{
  parameterVector = parameters;
  for(int p = 0; p < P; p++)
    *(this->parameters[p]) = parameters(p);
  for(std::vector<Layer*>::iterator layer = layers.begin();
      layer != layers.end(); layer++)
    (**layer).updatedParameters();
}

bool Net::providesInitialization()
{
  return true;
}

void Net::initialize()
{
  OPENANN_CHECK(initialized);
  for(std::vector<Layer*>::iterator layer = layers.begin();
      layer != layers.end(); layer++)
    (**layer).initializeParameters();
  for(int p = 0; p < P; p++)
    parameterVector(p) = *parameters[p];
}

double Net::error(unsigned int i)
{
  if(errorFunction == CE)
    return -(dataSet->getTarget(i).array() *
        (((*this)(dataSet->getInstance(i)).array() + 1e-10).log())).sum();
  else
    return ((*this)(dataSet->getInstance(i)) -
        dataSet->getTarget(i)).squaredNorm() / 2.0;
}

double Net::error()
{
  double e = 0.0;
  for(int n = 0; n < N; n++)
    e += error(n);

  if(errorFunction == MSE)
    return e / (double) N;
  else
    return e;
}

double Net::errorFromDataSet(DataSet& dataSet)
{
  double e = 0.0;
  if(errorFunction == CE)
  {
    for(int n = 0; n < dataSet.samples(); ++n)
      e -= (dataSet.getTarget(n).array() *
          (((*this)(dataSet.getInstance(n)).array() + 1e-10).log())).sum();
  }
  else
  {
    for(int n = 0; n < dataSet.samples(); ++n)
      e += ((*this)(dataSet.getInstance(n)) -
          dataSet.getTarget(n)).squaredNorm() / 2.0;
  }

  if(errorFunction == MSE)
    return e / (double) N;
  else
    return e;
}

bool Net::providesGradient()
{
  return true;
}

Eigen::VectorXd Net::gradient(unsigned int i)
{
  tempOutput = (*this)(dataSet->getInstance(i));
  tempError = (tempOutput - dataSet->getTarget(i)).transpose();
  Eigen::MatrixXd* e = &tempError;
  for(std::vector<Layer*>::reverse_iterator layer = layers.rbegin();
      layer != layers.rend(); layer++)
    (**layer).backpropagate(e, e);
  for(int i = 0; i < P; i++)
    tempGradient(i) = *derivatives[i];
  return tempGradient;
}

Eigen::VectorXd Net::gradient()
{
  tempGradient.fill(0.0);
  for(int n = 0; n < N; n++)
  {
    tempOutput = (*this)(dataSet->getInstance(n));
    tempError = (tempOutput - dataSet->getTarget(n)).transpose();
    Eigen::MatrixXd* e = &tempError;
    for(std::vector<Layer*>::reverse_iterator layer = layers.rbegin();
        layer != layers.rend(); layer++)
      (**layer).backpropagate(e, e);
    for(int i = 0; i < P; i++)
      tempGradient(i) += *derivatives[i];
  }
  if(errorFunction == MSE)
    tempGradient /= (double) dimension();
  return tempGradient;
}

void Net::errorGradient(int n, double& value, Eigen::VectorXd& grad)
{
  OPENANN_CHECK_EQUALS(grad.rows(), dimension());
  tempOutput = (*this)(dataSet->getInstance(n));
  tempError = (tempOutput - dataSet->getTarget(n)).transpose();
  if(errorFunction == CE)
    value = -(dataSet->getTarget(n).transpose().array() *
        ((tempOutput.array() + 1e-10).log())).sum();
  else
    value = tempError.squaredNorm() / 2.0;
  Eigen::MatrixXd* e = &tempError;
  for(std::vector<Layer*>::reverse_iterator layer = layers.rbegin();
      layer != layers.rend(); layer++)
    (**layer).backpropagate(e, e);
  for(int i = 0; i < P; i++)
    grad(i) = *derivatives[i];
}

bool Net::providesHessian()
{
  return false;
}

Eigen::MatrixXd Net::hessian()
{
  return Eigen::MatrixXd::Identity(dimension(), dimension());
}

}
