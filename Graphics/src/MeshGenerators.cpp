#include "stdafx.h"
#include "MeshGenerators.hpp"

namespace Graphics
{
	Mesh MeshGenerators::Quad(OpenGL* gl, Vector2 pos, Vector2 size /*= Vector2(1,1)*/, Rect uv /*= Rect(0, 0, 1, 1)*/)
	{
		Vector<SimpleVertex> verts =
		{
			{ { 0.0f,  size.y, 0.0f }, { uv.Left(), uv.Top() } },
			{ { size.x, 0.0f,  0.0f }, { uv.Right(), uv.Bottom() } },
			{ { size.x, size.y, 0.0f }, { uv.Right(), uv.Top() } },

			{ { 0.0f,  size.y, 0.0f }, { uv.Left(), uv.Top() } },
			{ { 0.0f,  0.0f,  0.0f }, { uv.Left(), uv.Bottom() } },
			{ { size.x, 0.0f,  0.0f }, { uv.Right(), uv.Bottom() } },
		};

		for(auto& v : verts)
		{
			v.pos += pos;
		}

		Mesh mesh = MeshRes::Create(gl);
		mesh->SetData(verts);
		mesh->SetPrimitiveType(PrimitiveType::TriangleList);
		return mesh;
	}

	void MeshGenerators::GenerateSimpleXYQuad(Rect3D r, Rect uv, Vector<SimpleVertex>& out)
	{
		Vector<MeshGenerators::SimpleVertex> verts =
		{
			{ { r.Left(),  r.Top(),     0.0f },{ uv.Left(), uv.Top() } },
			{ { r.Right(), r.Bottom(),  0.0f },{ uv.Right(), uv.Bottom() } },
			{ { r.Right(), r.Top(),     0.0f },{ uv.Right(), uv.Top() } },

			{ { r.Left(),  r.Top(),     0.0f },{ uv.Left(), uv.Top() } },
			{ { r.Left(),  r.Bottom(),  0.0f },{ uv.Left(), uv.Bottom() } },
			{ { r.Right(), r.Bottom(),  0.0f },{ uv.Right(), uv.Bottom() } },
		};
		for(auto& v : verts)
			out.Add(v);
	}
	void MeshGenerators::GenerateSimpleXZQuad(Rect3D r, Rect uv, Vector<MeshGenerators::SimpleVertex>& out)
	{
		Vector<MeshGenerators::SimpleVertex> verts =
		{
			{ { r.Left(),  0.0f, r.Top(),    },{ uv.Left(), uv.Top() } },
			{ { r.Right(), 0.0f, r.Bottom(), },{ uv.Right(), uv.Bottom() } },
			{ { r.Right(), 0.0f, r.Top(),    },{ uv.Right(), uv.Top() } },
						   
			{ { r.Left(),  0.0f, r.Top(),    },{ uv.Left(), uv.Top() } },
			{ { r.Left(),  0.0f, r.Bottom(), },{ uv.Left(), uv.Bottom() } },
			{ { r.Right(), 0.0f, r.Bottom(), },{ uv.Right(), uv.Bottom() } },
		};
		for(auto& v : verts)
			out.Add(v);
	}

}
