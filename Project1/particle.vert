// Copyright (c) 2017-2018, Samuel Ivan Gunadi
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#version 430 core
layout(std140, binding = 0) buffer Pos
{
	vec4 Positions[];
};
layout(std140, binding = 6) buffer Col
{
	vec4 Colors[];
};



layout(location = 0) in vec3 position;
out vec4 colorIn;

//out vec2 v_TexCoord;
//uniform mat4 u_M;
uniform mat4 u_V;
uniform mat4 u_P;


void main()
{



	mat4 modModel;
	// billboard the circle so it faces the camera and appears as a sphere
	modModel[3] = vec4(Positions[gl_InstanceID].xyz,1.0);



	modModel[2] = vec4(u_V[0][2], u_V[1][2], u_V[2][2], 0.0);
	modModel[1] = vec4(u_V[0][1], u_V[1][1], u_V[2][1], 0.0);
	modModel[0] = vec4(u_V[0][0], u_V[1][0], u_V[2][0], 0.0);

	

	
	gl_Position = (u_P * u_V * modModel) * vec4(position, 1.0);
	//change vertex color to make sphere shaded
	colorIn = vec4(Colors[gl_InstanceID].xyz * (1.0 + position.x) * (0.5 + position.y * 0.5) * 2.0, Colors[gl_InstanceID].w);

};