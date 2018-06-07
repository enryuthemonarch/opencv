// This file is part of OpenCV project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://opencv.org/license.html.

// Copyright (C) 2017, Intel Corporation, all rights reserved.
// Third party copyrights are property of their respective owners.
#include "../precomp.hpp"
#include "layers_common.hpp"
#include "../op_inf_engine.hpp"
#include <opencv2/imgproc.hpp>

namespace cv { namespace dnn {

class ResizeNearestNeighborLayerImpl CV_FINAL : public ResizeNearestNeighborLayer
{
public:
    ResizeNearestNeighborLayerImpl(const LayerParams& params)
    {
        setParamsFrom(params);
        CV_Assert(params.has("width") && params.has("height") || params.has("zoom_factor"));
        CV_Assert(!params.has("width") && !params.has("height") || !params.has("zoom_factor"));
        outWidth = params.get<float>("width", 0);
        outHeight = params.get<float>("height", 0);
        zoomFactor = params.get<int>("zoom_factor", 1);
        alignCorners = params.get<bool>("align_corners", false);
        if (alignCorners)
            CV_Error(Error::StsNotImplemented, "Nearest neighborhood resize with align_corners=true is not implemented");
    }

    bool getMemoryShapes(const std::vector<MatShape> &inputs,
                         const int requiredOutputs,
                         std::vector<MatShape> &outputs,
                         std::vector<MatShape> &internals) const CV_OVERRIDE
    {
        CV_Assert(inputs.size() == 1, inputs[0].size() == 4);
        outputs.resize(1, inputs[0]);
        outputs[0][2] = outHeight > 0 ? outHeight : (outputs[0][2] * zoomFactor);
        outputs[0][3] = outWidth > 0 ? outWidth : (outputs[0][3] * zoomFactor);
        // We can work in-place (do nothing) if input shape == output shape.
        return (outputs[0][2] == inputs[0][2]) && (outputs[0][3] == inputs[0][3]);
    }

    virtual bool supportBackend(int backendId) CV_OVERRIDE
    {
        return backendId == DNN_BACKEND_OPENCV ||
               backendId == DNN_BACKEND_INFERENCE_ENGINE && haveInfEngine();
    }

    virtual void finalize(const std::vector<Mat*>& inputs, std::vector<Mat> &outputs) CV_OVERRIDE
    {
        if (!outWidth && !outHeight)
        {
            outHeight = outputs[0].size[2];
            outWidth = outputs[0].size[3];
        }
    }

    void forward(InputArrayOfArrays inputs_arr, OutputArrayOfArrays outputs_arr, OutputArrayOfArrays internals_arr) CV_OVERRIDE
    {
        CV_TRACE_FUNCTION();
        CV_TRACE_ARG_VALUE(name, "name", name.c_str());

        Layer::forward_fallback(inputs_arr, outputs_arr, internals_arr);
    }

    void forward(std::vector<Mat*> &inputs, std::vector<Mat> &outputs, std::vector<Mat> &internals) CV_OVERRIDE
    {
        CV_TRACE_FUNCTION();
        CV_TRACE_ARG_VALUE(name, "name", name.c_str());

        if (outHeight == inputs[0]->size[2] && outWidth == inputs[0]->size[3])
            return;

        Mat& inp = *inputs[0];
        Mat& out = outputs[0];
        for (size_t n = 0; n < inputs[0]->size[0]; ++n)
        {
            for (size_t ch = 0; ch < inputs[0]->size[1]; ++ch)
            {
                resize(getPlane(inp, n, ch), getPlane(out, n, ch),
                       Size(outWidth, outHeight), 0, 0, INTER_NEAREST);
            }
        }
    }

    virtual Ptr<BackendNode> initInfEngine(const std::vector<Ptr<BackendWrapper> >&) CV_OVERRIDE
    {
#ifdef HAVE_INF_ENGINE
        InferenceEngine::LayerParams lp;
        lp.name = name;
        lp.type = "Resample";
        lp.precision = InferenceEngine::Precision::FP32;

        std::shared_ptr<InferenceEngine::CNNLayer> ieLayer(new InferenceEngine::CNNLayer(lp));
        ieLayer->params["type"] = "caffe.ResampleParameter.NEAREST";
        ieLayer->params["antialias"] = "0";
        ieLayer->params["width"] = cv::format("%d", outWidth);
        ieLayer->params["height"] = cv::format("%d", outHeight);

        return Ptr<BackendNode>(new InfEngineBackendNode(ieLayer));
#endif  // HAVE_INF_ENGINE
        return Ptr<BackendNode>();
    }

private:
    int outWidth, outHeight, zoomFactor;
    bool alignCorners;
};


Ptr<ResizeNearestNeighborLayer> ResizeNearestNeighborLayer::create(const LayerParams& params)
{
    return Ptr<ResizeNearestNeighborLayer>(new ResizeNearestNeighborLayerImpl(params));
}

}  // namespace dnn
}  // namespace cv