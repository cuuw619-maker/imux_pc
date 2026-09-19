cbuffer Camera : register(b0) {
    matrix worldViewProjection;
    float time;
    float3 padding;
};

struct VSInput {
    float3 position : POSITION;
    float3 normal : NORMAL;
    float4 color : COLOR0;
};

struct VSOutput {
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
    float4 color : COLOR0;
    float3 worldPosition : TEXCOORD0;
};

VSOutput VSMain(VSInput input) {
    VSOutput output;
    output.position = mul(float4(input.position, 1.0), worldViewProjection);
    output.normal = input.normal;
    output.color = input.color;
    output.worldPosition = input.position;
    return output;
}

float4 PSMain(VSOutput input) : SV_TARGET {
    float3 lightDirection = normalize(float3(-0.45, 0.85, -0.35));
    float diffuse = saturate(dot(normalize(input.normal), lightDirection)) * 0.65 + 0.35;
    float pulse = 0.025 * sin(time * 1.7 + input.worldPosition.x * 0.15);
    return float4(input.color.rgb * (diffuse + pulse), 1.0);
}
