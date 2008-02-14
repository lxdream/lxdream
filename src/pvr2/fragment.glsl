// Standard PVR2 fragment shader

void main()
{
	gl_FragColor = gl_Color;
	gl_FragDepth = gl_FragCoord.z;
}