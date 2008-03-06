// Standard PVR2 vertex shader

void main()
{
    gl_Position = ftransform();
//    gl_Position.z = log(gl_Vertex.z);
    gl_FrontColor = gl_Color;
    gl_FrontSecondaryColor = gl_SecondaryColor;
    gl_TexCoord[0] = gl_MultiTexCoord0;
}
