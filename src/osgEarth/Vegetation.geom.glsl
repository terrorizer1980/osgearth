#version $GLSL_VERSION_STR
$GLSL_DEFAULT_PRECISION_FLOAT
#pragma vp_location   geometry
#pragma vp_name VegetationGS
#pragma vp_entryPoint VegetationGeomShader
#extension GL_EXT_geometry_shader4 : enable
layout(points) in;
layout(triangle_strip) out;
layout(max_vertices = 4) out;
varying out float oe_billboard_brightness;
uniform float oe_billboard_width;
uniform float oe_billboard_height;
void VP_LoadVertex(in int);
void VP_EmitViewVertex();
vec3 vp_Normal;
vec4 vp_Color;
vec4 oe_sg_texcoord2;

void VegetationGeomShader(void)
{
    vec4 bb_color = vec4(1.0, 1.0, 1.0, 1.0);
    VP_LoadVertex(0);
	float brightness = vp_Normal.x;

	//double brightness at top verticies
	oe_billboard_brightness = brightness*2;
  
    float scale = vp_Normal.z;
    vec3 normal = vec3(0.0, 0.0, 1.0);
    vec4 base_pos = gl_ModelViewMatrix * gl_in[0].gl_Position;
    vec4 top_pos = gl_ModelViewMatrix * (gl_in[0].gl_Position + vec4(normal * oe_billboard_height * scale, 0.0));
    //TODO: this width calculation isn't great but works for now
	vec4 center_v = gl_ModelViewMatrix * vec4(0.,0.,0.,1.);
    vec4 right_v = gl_ModelViewMatrix * vec4(oe_billboard_width * scale,0.,0.,1.);
    float width = distance(right_v, center_v);
    
	gl_Position = (top_pos + vec4(-width, 0.0, 0.0, 0.0));
    oe_sg_texcoord2 = vec4(0.0, 1.0, 0.0, 0.0);
    vp_Normal =  normal;
    vp_Color = bb_color;
    VP_EmitViewVertex();
    
	gl_Position = (top_pos + vec4(width, 0.0, 0.0, 0.0));
    oe_sg_texcoord2 = vec4(1.0, 1.0, 0.0, 0.0);
    vp_Normal =  normal;
    vp_Color = bb_color;
    VP_EmitViewVertex();

	//Orignal brightness at base
    oe_billboard_brightness = brightness;
    
	gl_Position = (base_pos + vec4(-width, 0.0, 0.0, 0.0));
    oe_sg_texcoord2 = vec4(0.0, 0.0, 0.0, 0.0);
    vp_Normal =  normal;
    vp_Color = bb_color;
    VP_EmitViewVertex();
    
	gl_Position = (base_pos + vec4(width, 0.0, 0.0, 0.0));
    oe_sg_texcoord2 = vec4(1.0, 0.0, 0.0, 0.0);
    vp_Normal =  normal;
    vp_Color = bb_color;
    VP_EmitViewVertex();

    EndPrimitive();
}
