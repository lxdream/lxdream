// Standard PVR2 vertex shader

void main()
{
    vec4 tmp = ftransform();
    float w = gl_Vertex.z;
    gl_Position  = tmp * w;
    gl_FrontColor = gl_Color;
    gl_FrontSecondaryColor = gl_SecondaryColor;
    gl_TexCoord[0] = gl_MultiTexCoord0;
}
