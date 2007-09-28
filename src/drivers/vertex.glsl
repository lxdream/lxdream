// Standard PVR2 vertex shader

void main()
{
	gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
}
