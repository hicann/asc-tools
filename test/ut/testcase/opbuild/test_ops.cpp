/**
* Copyright (c) 2025 Huawei Technologies Co., Ltd.
* This program is free software, you can redistribute it and/or modify it under the terms and conditions of
* CANN Open Software License Agreement Version 2.0 (the "License").
* Please refer to the License for details. You may not use this file except in compliance with the License.
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
* INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
* See LICENSE in the root of the software repository for the full text of the License.
*/
#include "register/op_def_registry.h"

namespace optiling {
struct Tik2AddTilingCompileInfo {
    int64_t core_num;
    int64_t ub_size;
};

ge::graphStatus TilingTik2Add(gert::TilingContext* context)
{
    return ge::GRAPH_SUCCESS;
}

ge::graphStatus check_op_support(const ge::Operator& op, ge::AscendString& result)
{
    return ge::GRAPH_SUCCESS;
}

ge::graphStatus get_op_support(const ge::Operator& op, ge::AscendString& result)
{
    return ge::GRAPH_SUCCESS;
}

ge::graphStatus op_select_format(const ge::Operator& op, ge::AscendString& result)
{
    return ge::GRAPH_SUCCESS;
}

ge::graphStatus get_op_specific_info(const ge::Operator& op, ge::AscendString& result)
{
    return ge::GRAPH_SUCCESS;
}

ge::graphStatus generalize_config(const ge::Operator& op, const ge::AscendString& generalize_config,
    ge::AscendString& generalize_para)
{
    return ge::GRAPH_SUCCESS;
}
} // namespace optiling

// 算子原型及接口定义开始
// infershape/tiling/

namespace ge {
ge::graphStatus InferShape4AddTik2(gert::InferShapeContext* context)
{
    return GRAPH_SUCCESS;
}

ge::graphStatus InferShapeRange4AddTik2(gert::InferShapeRangeContext* context)
{
    return GRAPH_SUCCESS;
}

ge::graphStatus InferDataType4AddTik2(gert::InferDataTypeContext* context)
{
    return GRAPH_SUCCESS;
}
} // namespace ge

namespace ops {
class AddTik2 : public OpDef {
public:
    AddTik2(const char* name) : OpDef(name)
    {
        this->Input("x1").DataType({ ge::DT_FLOAT16 }).ParamType(OPTIONAL);
        this->Input("x2").DataType({ ge::DT_FLOAT16 }).ParamType(DYNAMIC);
        this->Output("y").DataType({ ge::DT_FLOAT16 });
        this->Output("y2").DataType({ ge::DT_FLOAT16 }).ParamType(DYNAMIC);
        this->SetInferShape(ge::InferShape4AddTik2)
            .SetInferShapeRange(ge::InferShapeRange4AddTik2)
            .SetInferDataType(ge::InferDataType4AddTik2);
        this->Attr("bias0").AttrType(OPTIONAL).Int(0);
        this->Attr("bias1").AttrType(OPTIONAL).Float(0.0);
        this->Attr("bias2").AttrType(OPTIONAL).ListBool({ true, false });
        this->Attr("bias3").AttrType(OPTIONAL).ListFloat({ 0.1, 0.2 });
        this->Attr("bias4").AttrType(OPTIONAL).ListInt({ 1, 2 });
        this->Attr("bias5").AttrType(OPTIONAL).String("ssss");
        this->Attr("bias6").AttrType(OPTIONAL).Bool(true);
        this->Attr("bias00").Int();
        this->Attr("bias11").Float();
        this->Attr("bias22").ListBool({ true, false });
        this->Attr("bias33").ListFloat({ 0.1, 0.2 });
        this->Attr("bias44").ListInt({ 1, 2 });
        this->Attr("bias55").String("ssss");
        this->Attr("bias66").Bool();
        this->AICore()
            .SetTiling(optiling::TilingTik2Add)
            .SetCheckSupport(optiling::check_op_support)
            .SetOpSelectFormat(optiling::op_select_format)
            .SetOpSupportInfo(optiling::get_op_support)
            .SetOpSpecInfo(optiling::get_op_specific_info)
            .SetParamGeneralize(optiling::generalize_config);
        OpAICoreConfig aicConfig;
        aicConfig.Input("x1")
            .ParamType(OPTIONAL)
            .DataType({ ge::DT_FLOAT16, ge::DT_FLOAT })
            .Format({ ge::FORMAT_ND, ge::FORMAT_ND })
            .UnknownShapeFormat({ ge::FORMAT_ND, ge::FORMAT_ND })
            .ValueDepend(REQUIRED);
        aicConfig.Input("x2")
            .ParamType(DYNAMIC)
            .DataType({ ge::DT_FLOAT16, ge::DT_FLOAT })
            .Format({ ge::FORMAT_ND, ge::FORMAT_NCHW })
            .UnknownShapeFormat({ ge::FORMAT_ND, ge::FORMAT_NCHW });
        aicConfig.Output("y")
            .ParamType(REQUIRED)
            .DataType({ ge::DT_FLOAT16, ge::DT_FLOAT })
            .Format({ ge::FORMAT_ND, ge::FORMAT_NCL })
            .UnknownShapeFormat({ ge::FORMAT_ND, ge::FORMAT_NCL });
        aicConfig.Output("y2")
            .ParamType(DYNAMIC)
            .DataType({ ge::DT_FLOAT16, ge::DT_FLOAT })
            .Format({ ge::FORMAT_ND, ge::FORMAT_ND })
            .UnknownShapeFormat({ ge::FORMAT_ND, ge::FORMAT_ND });
        aicConfig.DynamicCompileStaticFlag(true)
            .DynamicFormatFlag(true)
            .DynamicRankSupportFlag(true)
            .DynamicShapeSupportFlag(true)
            .NeedCheckSupportFlag(true)
            .PrecisionReduceFlag(true);
        this->AICore().AddConfig("ascend310p", aicConfig);
        this->AICore().AddConfig("ascend910", aicConfig);
        this->FormatMatchMode(FormatCheckOption::STRICT);
    }
};

OP_ADD(AddTik2, optiling::Tik2AddTilingCompileInfo);

class InputTest : public OpDef {
public:
    InputTest(const char* name) : OpDef(name)
    {
        this->Input("x1").DataType({ ge::DT_FLOAT16 });
        this->Input("x2").DataType({ ge::DT_FLOAT16 }).ParamType(DYNAMIC);
        OpAICoreConfig aicConfig;
        this->AICore().AddConfig("ascend310p", aicConfig);
        this->AICore().AddConfig("ascend910", aicConfig);
    }
};

OP_ADD(InputTest);

class OutputTest : public OpDef {
public:
    OutputTest(const char* name) : OpDef(name)
    {
        this->Output("x1").DataType({ ge::DT_FLOAT16 });
        this->Output("x2").DataType({ ge::DT_FLOAT16 }).ParamType(DYNAMIC);
        OpAICoreConfig aicConfig;
        this->AICore().AddConfig("ascend310p", aicConfig);
        this->AICore().AddConfig("ascend910", aicConfig);
    }
};

OP_ADD(OutputTest);

class ValueDependTest : public OpDef {
public:
    ValueDependTest(const char* name) : OpDef(name)
    {
        this->Input("x1").DataType({ ge::DT_FLOAT }).ValueDepend(REQUIRED);
        this->Input("x2").DataType({ ge::DT_INT64 }).ValueDepend(REQUIRED);
        this->Input("x3").DataType({ ge::DT_BOOL }).ParamType(OPTIONAL).ValueDepend(OPTIONAL);
        this->Output("y").DataType({ ge::DT_FLOAT });
        OpAICoreConfig aicConfig;
        this->AICore().AddConfig("ascend910", aicConfig);
    }
};

OP_ADD(ValueDependTest);

class AutoContiguousTest : public OpDef {
public:
    AutoContiguousTest(const char* name) : OpDef(name)
    {
        this->Input("x1").DataType({ ge::DT_FLOAT }).AutoContiguous();
        this->Input("x2").DataType({ ge::DT_INT64 }).AutoContiguous();
        this->Output("y").DataType({ ge::DT_FLOAT });
        OpAICoreConfig aicConfig;
        this->AICore().AddConfig("ascend910", aicConfig);
    }
};

OP_ADD(AutoContiguousTest);

class ScalarTest : public OpDef {
public:
    ScalarTest(const char* name) : OpDef(name)
    {
        this->Input("x1").DataType({ ge::DT_INT64 });
        this->Input("x2").DataType({ ge::DT_FLOAT }).Scalar();
        this->Input("x3").DataType({ ge::DT_FLOAT }).Scalar().To(ge::DT_INT64);
        this->Input("x4").DataType({ ge::DT_INT32 }).Scalar().To("x1");
        this->Input("x5").DataType({ ge::DT_INT64 }).ScalarList();
        this->Input("x6").DataType({ ge::DT_INT64 }).ScalarList().To(ge::DT_INT32);
        this->Input("x7").DataType({ ge::DT_INT64 }).ScalarList().To("x6");
        this->Input("x8").DataType({ ge::DT_FLOAT });
        this->Output("y").DataType({ ge::DT_FLOAT });
        OpAICoreConfig aicConfig;
        this->AICore().AddConfig("ascend910", aicConfig);
    }
};

OP_ADD(ScalarTest);

class RefTest : public OpDef {
public:
    RefTest(const char* name) : OpDef(name)
    {
        this->Input("self").DataType({ ge::DT_FLOAT });
        this->Input("x").DataType({ ge::DT_INT64 });
        this->Output("self").DataType({ ge::DT_FLOAT });
        OpAICoreConfig aicConfig;
        this->AICore().AddConfig("ascend910", aicConfig);
    }
};

OP_ADD(RefTest);

class RefContiguousTest : public OpDef {
public:
    RefContiguousTest(const char* name) : OpDef(name)
    {
        this->Input("self").DataType({ ge::DT_FLOAT }).AutoContiguous();
        this->Input("x").DataType({ ge::DT_INT64 });
        this->Output("self").DataType({ ge::DT_FLOAT });
        OpAICoreConfig aicConfig;
        this->AICore().AddConfig("ascend910", aicConfig);
    }
};

OP_ADD(RefContiguousTest);

class DynamicRefTest : public OpDef {
public:
    DynamicRefTest(const char* name) : OpDef(name)
    {
        this->Input("dynamicSelf").DataType({ ge::DT_FLOAT }).ParamType(DYNAMIC);
        this->Input("x").DataType({ ge::DT_INT64 });
        this->Output("dynamicSelf").DataType({ ge::DT_FLOAT }).ParamType(DYNAMIC);
        OpAICoreConfig aicConfig;
        this->AICore().AddConfig("ascend910", aicConfig);
    }
};

OP_ADD(DynamicRefTest);

class VersionTest : public OpDef {
public:
    VersionTest(const char* name) : OpDef(name)
    {
        this->Input("x1").DataType({ ge::DT_FLOAT });
        this->Input("x2").DataType({ ge::DT_INT64 });
        this->Input("x3").ParamType(OPTIONAL).DataType({ ge::DT_INT64 }).Version(1);
        this->Input("x4").ParamType(OPTIONAL).DataType({ ge::DT_INT64 }).Version(2);
        this->Output("y").DataType({ ge::DT_FLOAT });
        this->Output("x1").DataType({ ge::DT_FLOAT });
        this->Attr("bias1").AttrType(OPTIONAL).Int(1).Version(1);
        this->Attr("bias2").AttrType(OPTIONAL).Float(0.0).Version(2);
        this->Attr("bias3").AttrType(OPTIONAL).Bool(true).Version(2);
        this->Attr("bias4").AttrType(OPTIONAL).ListBool({ true, false }).Version(1);
        this->Attr("bias5").AttrType(OPTIONAL).ListFloat({ 0.1, 0.2 }).Version(1);
        this->Attr("bias6").AttrType(OPTIONAL).ListInt({ 1, 2 }).Version(2);
        this->Attr("bias7").AttrType(OPTIONAL).String("ssss").Version(2);
        OpAICoreConfig aicConfig;
        aicConfig.ExtendCfgInfo("aclnnSupport.value", "aclnn_only");
        this->AICore().AddConfig("ascend910", aicConfig);
        this->EnableFallBack();
    }
};

OP_ADD(VersionTest);

class MC2Test : public OpDef {
public:
    MC2Test(const char* name) : OpDef(name)
    {
        this->Input("x").ParamType(REQUIRED).DataType({ ge::DT_FLOAT }).Format({ ge::FORMAT_ND });
        this->Output("y").ParamType(REQUIRED).DataType({ ge::DT_FLOAT }).Format({ ge::FORMAT_ND });
        this->Attr("group1").AttrType(REQUIRED).String();
        this->Attr("group2").AttrType(OPTIONAL).String();
        OpAICoreConfig aicConfig;
        this->AICore().AddConfig("ascend910", aicConfig);
        this->AICore().AddConfig("ascend910b", aicConfig);
        this->MC2().HcclGroup({"group2", "group1"});
        this->MC2().HcclServerType(HcclServerType::AICPU, "ascend");
        this->MC2().HcclServerType(HcclServerType::AICPU, "ascend910");
        this->MC2().HcclServerType(HcclServerType::AICORE);
        this->EnableFallBack();
    }
};
 
OP_ADD(MC2Test);

class MC2TestDavid : public OpDef {
public:
    MC2TestDavid(const char* name) : OpDef(name)
    {
        this->Input("x").ParamType(REQUIRED).DataType({ ge::DT_FLOAT }).Format({ ge::FORMAT_ND });
        this->Output("y").ParamType(REQUIRED).DataType({ ge::DT_FLOAT }).Format({ ge::FORMAT_ND });
        this->Attr("group1").AttrType(REQUIRED).String();
        this->Attr("group2").AttrType(OPTIONAL).String();
        OpAICoreConfig aicConfig;
        this->AICore().AddConfig("ascend910_95", aicConfig);
        this->MC2().HcclGroup({"group2", "group1"});
        this->MC2().HcclServerType(HcclServerType::CCU, "ascend910_95");
        this->EnableFallBack();
    }
};
 
OP_ADD(MC2TestDavid);

class ValueDependScopeTest : public OpDef {
public:
    ValueDependScopeTest(const char* name) : OpDef(name)
    {
        this->Input("x1").DataType({ ge::DT_FLOAT }).ValueDepend(REQUIRED, DependScope::TILING);
        this->Input("x2").DataType({ ge::DT_INT64 }).ValueDepend(OPTIONAL, DependScope::TILING);
        this->Input("x3").DataType({ ge::DT_BOOL }).ParamType(OPTIONAL).ValueDepend(OPTIONAL);
        this->Output("y").DataType({ ge::DT_FLOAT });
        OpAICoreConfig aicConfig;
        this->AICore().AddConfig("ascend910", aicConfig);
    }
};

OP_ADD(ValueDependScopeTest);

class OutShapeDepOnCompute : public OpDef {
public:
    OutShapeDepOnCompute(const char* name) : OpDef(name)
    {
        this->Input("x1").ParamType(REQUIRED).DataType({ ge::DT_INT64 }).ValueDepend(REQUIRED);;
        this->Input("x2").ParamType(REQUIRED).DataType({ ge::DT_INT64 }).ValueDepend(REQUIRED);;
        this->Output("y1").ParamType(REQUIRED).DataType({ ge::DT_INT64 }).OutputShapeDependOnCompute();
        this->Output("y2").ParamType(REQUIRED).DataType({ ge::DT_INT64 });
        this->Output("y3").ParamType(DYNAMIC).DataType({ ge::DT_INT64 });
        OpAICoreConfig aicConfig;
        this->AICore().AddConfig("ascend910", aicConfig);
    }
};
OP_ADD(OutShapeDepOnCompute);

// 框架插件接口及定义结束
} // namespace ops
