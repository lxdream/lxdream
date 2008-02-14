// Standard PVR2 vertex shader

void main()
{
    gl_Position.xy = ftransform().xy;
    gl_Position.z = gl_Vertex.z;
    gl_Position.w = 1;
    gl_FrontColor = gl_Color;
    gl_FrontSecondaryColor = gl_SecondaryColor;
    gl_TexCoord[0] = gl_MultiTexCoord0;
}
