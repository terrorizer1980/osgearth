#version $GLSL_VERSION_STR
#pragma vp_name VegetationFS
#pragma vp_entryPoint VegetationFragShaderColoring
#pragma vp_location fragment_coloring
varying float oe_billboard_brightness;
uniform float oe_thermal_mode;
uniform float oe_veg_temperature_detail_strength = 1.0;
uniform float oe_veg_temperature = 275;
void VegetationFragShaderColoring(inout vec4 color) 
{
    //if (color.a < 0.2) discard; 
    float contrast = clamp(1.0 - oe_billboard_brightness, 0.85, 1.0);
    color.rgb = clamp(((color.rgb-0.5)*contrast + 0.5) * (1.0+oe_billboard_brightness), 0.0, 1.0);
    if(oe_thermal_mode > 0)
    {
        color.r  = ((1.0 - color.r) + (1.0 - color.g) + (1.0 - color.b))/3.0;
        color.r = oe_veg_temperature + color.r*oe_veg_temperature_detail_strength;
        color.g = color.r;
        color.b = color.r;
        if ( color.a < 0.2 ) discard;
    }
};
