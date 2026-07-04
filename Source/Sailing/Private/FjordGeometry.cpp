#include "FjordGeometry.h"
#include "ProceduralMeshComponent.h"
#include "KismetProceduralMeshLibrary.h"
#include "GeomTools.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"

namespace FjordGeometry
{
	// -----------------------------------------------------------------------
	// FHeightGrid
	// -----------------------------------------------------------------------

	float FHeightGrid::SampleMeters(float WorldX, float WorldY) const
	{
		if (!IsValid())
		{
			return 0.0f;
		}

		// World XY -> normalized [0,1] within the bbox.
		const float U = (WorldX - MinX) / FMath::Max(MaxX - MinX, KINDA_SMALL_NUMBER);
		float V = (WorldY - MinY) / FMath::Max(MaxY - MinY, KINDA_SMALL_NUMBER);
		// Row 0 is north (max Y); world Y grows north, so flip V into row space.
		if (bRow0IsNorth)
		{
			V = 1.0f - V;
		}

		// Continuous grid coords, clamped to valid sampling range.
		const float Gx = FMath::Clamp(U, 0.0f, 1.0f) * (Width - 1);
		const float Gy = FMath::Clamp(V, 0.0f, 1.0f) * (Height - 1);

		const int32 X0 = FMath::FloorToInt(Gx);
		const int32 Y0 = FMath::FloorToInt(Gy);
		const int32 X1 = FMath::Min(X0 + 1, Width - 1);
		const int32 Y1 = FMath::Min(Y0 + 1, Height - 1);
		const float Fx = Gx - X0;
		const float Fy = Gy - Y0;

		auto At = [this](int32 X, int32 Y) -> float
		{
			const uint16 U16 = Samples[Y * Width + X];
			return MinMeters + (static_cast<float>(U16) / 65535.0f) * (MaxMeters - MinMeters);
		};

		const float A = At(X0, Y0);
		const float B = At(X1, Y0);
		const float C = At(X0, Y1);
		const float D = At(X1, Y1);
		const float Top = FMath::Lerp(A, B, Fx);
		const float Bot = FMath::Lerp(C, D, Fx);
		return FMath::Lerp(Top, Bot, Fy);
	}

	TSharedPtr<FHeightGrid> FHeightGrid::Load(const FString& Name)
	{
		const FString Base = FPaths::ProjectContentDir() / TEXT("Fjord") / TEXT("Terrain") / Name;
		const FString JsonPath = Base + TEXT(".json");
		const FString R16Path = Base + TEXT(".r16");

		FString JsonText;
		if (!FFileHelper::LoadFileToString(JsonText, *JsonPath))
		{
			UE_LOG(LogTemp, Warning, TEXT("FHeightGrid: no metadata at %s"), *JsonPath);
			return nullptr;
		}

		TSharedPtr<FJsonObject> Root;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			UE_LOG(LogTemp, Warning, TEXT("FHeightGrid: bad JSON at %s"), *JsonPath);
			return nullptr;
		}

		TSharedPtr<FHeightGrid> Grid = MakeShared<FHeightGrid>();
		Grid->Width = Root->GetIntegerField(TEXT("width"));
		Grid->Height = Root->GetIntegerField(TEXT("height"));
		Grid->MinMeters = static_cast<float>(Root->GetNumberField(TEXT("min_m")));
		Grid->MaxMeters = static_cast<float>(Root->GetNumberField(TEXT("max_m")));
		Grid->bRow0IsNorth = Root->HasField(TEXT("row0_is_north")) ? Root->GetBoolField(TEXT("row0_is_north")) : true;

		const TSharedPtr<FJsonObject>* Bbox = nullptr;
		if (Root->TryGetObjectField(TEXT("bbox_unreal"), Bbox) && Bbox)
		{
			Grid->MinX = static_cast<float>((*Bbox)->GetNumberField(TEXT("min_x")));
			Grid->MinY = static_cast<float>((*Bbox)->GetNumberField(TEXT("min_y")));
			Grid->MaxX = static_cast<float>((*Bbox)->GetNumberField(TEXT("max_x")));
			Grid->MaxY = static_cast<float>((*Bbox)->GetNumberField(TEXT("max_y")));
		}

		TArray<uint8> Bytes;
		if (!FFileHelper::LoadFileToArray(Bytes, *R16Path))
		{
			UE_LOG(LogTemp, Warning, TEXT("FHeightGrid: no heightfield at %s"), *R16Path);
			return nullptr;
		}
		const int32 Expected = Grid->Width * Grid->Height;
		if (Bytes.Num() < Expected * 2)
		{
			UE_LOG(LogTemp, Warning, TEXT("FHeightGrid: %s too small (%d bytes, need %d)"),
				*R16Path, Bytes.Num(), Expected * 2);
			return nullptr;
		}
		Grid->Samples.SetNumUninitialized(Expected);
		FMemory::Memcpy(Grid->Samples.GetData(), Bytes.GetData(), Expected * 2);

		if (!Grid->IsValid())
		{
			return nullptr;
		}
		UE_LOG(LogTemp, Log, TEXT("FHeightGrid: loaded %dx%d, %.1f..%.1f m."),
			Grid->Width, Grid->Height, Grid->MinMeters, Grid->MaxMeters);
		return Grid;
	}

	// -----------------------------------------------------------------------
	// Polygon builders
	// -----------------------------------------------------------------------

	// Drop a duplicated closing vertex if present (OSM rings are closed).
	static void DropClosingVertex(TArray<FVector2D>& Ring)
	{
		if (Ring.Num() >= 2 && Ring[0].Equals(Ring.Last(), 0.01f))
		{
			Ring.Pop();
		}
	}

	bool BuildFilledPolygon(
		UProceduralMeshComponent* Mesh,
		const TArray<FVector2D>& InRing,
		float TopZ,
		float SkirtDepth,
		int32 Section,
		const FColor& Color)
	{
		if (!Mesh || InRing.Num() < 3)
		{
			return false;
		}

		TArray<FVector2D> Ring = InRing;
		DropClosingVertex(Ring);
		if (Ring.Num() < 3)
		{
			return false;
		}

		// Triangulate the (possibly concave) polygon into flat triples of 2D points.
		TArray<FVector2D> TriPoints;
		if (!FGeomTools2D::TriangulatePoly(TriPoints, Ring, /*bKeepColinearVertices*/ false)
			|| TriPoints.Num() < 3)
		{
			return false;
		}

		TArray<FVector> Vertices;
		TArray<int32> Triangles;
		TArray<FVector> Normals;
		TArray<FVector2D> UVs;
		TArray<FColor> VertexColors;

		// --- Top face -------------------------------------------------------
		for (int32 t = 0; t + 2 < TriPoints.Num(); t += 3)
		{
			const FVector2D& A = TriPoints[t];
			const FVector2D& B = TriPoints[t + 1];
			const FVector2D& C = TriPoints[t + 2];

			const int32 Base = Vertices.Num();
			Vertices.Add(FVector(A.X, A.Y, TopZ));
			Vertices.Add(FVector(B.X, B.Y, TopZ));
			Vertices.Add(FVector(C.X, C.Y, TopZ));
			for (int32 k = 0; k < 3; ++k)
			{
				Normals.Add(FVector::UpVector);
				VertexColors.Add(Color);
			}
			UVs.Add(A * 0.001f);
			UVs.Add(B * 0.001f);
			UVs.Add(C * 0.001f);

			const float Signed = (B.X - A.X) * (C.Y - A.Y) - (B.Y - A.Y) * (C.X - A.X);
			if (Signed > 0.0f)
			{
				Triangles.Add(Base); Triangles.Add(Base + 2); Triangles.Add(Base + 1);
			}
			else
			{
				Triangles.Add(Base); Triangles.Add(Base + 1); Triangles.Add(Base + 2);
			}
		}

		// --- Skirt (vertical wall around the outline) -----------------------
		if (SkirtDepth > 0.0f)
		{
			const float BotZ = TopZ - SkirtDepth;
			const int32 N = Ring.Num();
			for (int32 i = 0; i < N; ++i)
			{
				const FVector2D& P0 = Ring[i];
				const FVector2D& P1 = Ring[(i + 1) % N];
				FVector2D Edge = P1 - P0;
				Edge.Normalize();
				const FVector Outward(Edge.Y, -Edge.X, 0.0f);

				const int32 Base = Vertices.Num();
				Vertices.Add(FVector(P0.X, P0.Y, TopZ));
				Vertices.Add(FVector(P1.X, P1.Y, TopZ));
				Vertices.Add(FVector(P0.X, P0.Y, BotZ));
				Vertices.Add(FVector(P1.X, P1.Y, BotZ));
				for (int32 k = 0; k < 4; ++k)
				{
					Normals.Add(Outward);
					VertexColors.Add(Color);
				}
				UVs.Add(FVector2D(0.0f, 0.0f));
				UVs.Add(FVector2D(1.0f, 0.0f));
				UVs.Add(FVector2D(0.0f, 1.0f));
				UVs.Add(FVector2D(1.0f, 1.0f));

				Triangles.Add(Base); Triangles.Add(Base + 2); Triangles.Add(Base + 1);
				Triangles.Add(Base + 1); Triangles.Add(Base + 2); Triangles.Add(Base + 3);
			}
		}

		Mesh->CreateMeshSection(Section, Vertices, Triangles, Normals, UVs, VertexColors,
			TArray<FProcMeshTangent>(), /*bCreateCollision*/ true);
		return true;
	}

	bool BuildTerrainPolygon(
		UProceduralMeshComponent* Mesh,
		const TArray<FVector2D>& InRing,
		const FHeightGrid& Grid,
		const FVector2D& WorldOrigin,
		float BaseZ,
		float HeightScale,
		float SkirtDepth,
		int32 Section,
		const FColor& Color)
	{
		if (!Mesh || InRing.Num() < 3 || !Grid.IsValid())
		{
			return false;
		}

		TArray<FVector2D> Ring = InRing;
		DropClosingVertex(Ring);
		if (Ring.Num() < 3)
		{
			return false;
		}

		// Local Z = sea-level base + real DTM elevation (meters, clamped at 0) at the world
		// position. The polygon is triangulated (exact OSM shore) and each triangle is then
		// subdivided so interior points sample real elevation too, giving actual relief.
		auto LocalZ = [&](const FVector2D& Local) -> float
		{
			const float Elev = Grid.SampleMeters(WorldOrigin.X + Local.X, WorldOrigin.Y + Local.Y);
			return BaseZ + FMath::Max(Elev, 0.0f) * HeightScale;
		};

		TArray<FVector2D> TriPoints;
		if (!FGeomTools2D::TriangulatePoly(TriPoints, Ring, /*bKeepColinearVertices*/ false)
			|| TriPoints.Num() < 3)
		{
			return false;
		}

		TArray<FVector> Vertices;
		TArray<int32> Triangles;
		TArray<FVector2D> UVs;
		TArray<FColor> VertexColors;

		// Shared-vertex mesh: verts at the same quantized position share an index, so
		// CalculateTangentsForMesh below can average face normals into smooth vertex normals
		// (no faceting) and produce tangents (no "melting" shading). The top surface is a
		// heightfield (one Z per XY); the skirt top edge shares XY+Z with the shore verts, so
		// the shore edge rounds off smoothly instead of showing a hard black seam.
		TMap<FIntVector, int32> VertMap;
		auto AddVert = [&](const FVector& V) -> int32
		{
			const FIntVector Key(FMath::RoundToInt(V.X), FMath::RoundToInt(V.Y), FMath::RoundToInt(V.Z));
			if (const int32* Found = VertMap.Find(Key))
			{
				return *Found;
			}
			const int32 Idx = Vertices.Num();
			Vertices.Add(V);
			UVs.Add(FVector2D(V.X, V.Y) * 0.001f); // cosmetic; M_Land maps by world position
			VertexColors.Add(Color);
			VertMap.Add(Key, Idx);
			return Idx;
		};

		// Emit one up-facing, elevation-displaced top triangle from three 2D points.
		auto EmitTri = [&](const FVector2D& A, const FVector2D& B, const FVector2D& C)
		{
			const int32 IA = AddVert(FVector(A.X, A.Y, LocalZ(A)));
			const int32 IB = AddVert(FVector(B.X, B.Y, LocalZ(B)));
			const int32 IC = AddVert(FVector(C.X, C.Y, LocalZ(C)));
			if (IA == IB || IB == IC || IA == IC)
			{
				return; // degenerate (collapsed) triangle — skip so it can't pollute normals
			}
			const float Signed = (B.X - A.X) * (C.Y - A.Y) - (B.Y - A.Y) * (C.X - A.X);
			if (Signed > 0.0f)
			{
				Triangles.Add(IA); Triangles.Add(IC); Triangles.Add(IB);
			}
			else
			{
				Triangles.Add(IA); Triangles.Add(IB); Triangles.Add(IC);
			}
		};

		// Uniform subdivision level for this ring: sample the interior (not just the shore)
		// so real hills appear. Uniform per ring => watertight (this ring is one section).
		FVector2D BMin(FLT_MAX, FLT_MAX), BMax(-FLT_MAX, -FLT_MAX);
		for (const FVector2D& P : Ring)
		{
			BMin.X = FMath::Min(BMin.X, P.X);
			BMin.Y = FMath::Min(BMin.Y, P.Y);
			BMax.X = FMath::Max(BMax.X, P.X);
			BMax.Y = FMath::Max(BMax.Y, P.Y);
		}
		const float MaxDim = FMath::Max(BMax.X - BMin.X, BMax.Y - BMin.Y);
		const float TargetEdge = 3400.0f; // ~34 m, matching the DTM cell size
		const int32 MaxLevel = 4;          // cap sub-triangle explosion (<= 16x per triangle)
		const int32 Lvl = FMath::Clamp(FMath::CeilToInt(MaxDim / TargetEdge), 1, MaxLevel);

		for (int32 t = 0; t + 2 < TriPoints.Num(); t += 3)
		{
			const FVector2D& A = TriPoints[t];
			const FVector2D& B = TriPoints[t + 1];
			const FVector2D& C = TriPoints[t + 2];

			if (Lvl <= 1)
			{
				EmitTri(A, B, C);
				continue;
			}

			// Barycentric lattice with Lvl+1 points per side; emit its sub-triangles.
			const float Inv = 1.0f / Lvl;
			auto P = [&](int32 i, int32 j) -> FVector2D
			{
				const float wa = (Lvl - i - j) * Inv;
				const float wb = i * Inv;
				const float wc = j * Inv;
				return A * wa + B * wb + C * wc;
			};
			for (int32 i = 0; i < Lvl; ++i)
			{
				for (int32 j = 0; j < Lvl - i; ++j)
				{
					EmitTri(P(i, j), P(i + 1, j), P(i, j + 1));
					if (i + j < Lvl - 1)
					{
						EmitTri(P(i + 1, j), P(i + 1, j + 1), P(i, j + 1));
					}
				}
			}
		}

		// Skirt: from the (displaced) shore Z down to a constant depth below sea level.
		// Top corners share verts with the shore surface (rounds the shore edge); bottom
		// corners are shared between adjacent segments (smooth wall).
		if (SkirtDepth > 0.0f)
		{
			const float BotZ = BaseZ - SkirtDepth;
			const int32 N = Ring.Num();
			for (int32 i = 0; i < N; ++i)
			{
				const FVector2D& P0 = Ring[i];
				const FVector2D& P1 = Ring[(i + 1) % N];

				const int32 T0 = AddVert(FVector(P0.X, P0.Y, LocalZ(P0)));
				const int32 T1 = AddVert(FVector(P1.X, P1.Y, LocalZ(P1)));
				const int32 B0 = AddVert(FVector(P0.X, P0.Y, BotZ));
				const int32 B1 = AddVert(FVector(P1.X, P1.Y, BotZ));
				if (T0 == T1)
				{
					continue; // zero-length edge
				}

				Triangles.Add(T0); Triangles.Add(B0); Triangles.Add(T1);
				Triangles.Add(T1); Triangles.Add(B0); Triangles.Add(B1);
			}
		}

		// Smooth vertex normals + tangents from the shared-vertex mesh.
		TArray<FVector> Normals;
		TArray<FProcMeshTangent> Tangents;
		UKismetProceduralMeshLibrary::CalculateTangentsForMesh(Vertices, Triangles, UVs, Normals, Tangents);

		Mesh->CreateMeshSection(Section, Vertices, Triangles, Normals, UVs, VertexColors,
			Tangents, /*bCreateCollision*/ true);
		return true;
	}
}
