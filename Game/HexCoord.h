#pragma once
#include <array>
#include <cstdlib>

/// <summary>
/// ヘックスグリッドの座標（axial座標系）。
/// </summary>
struct HexCoord
{
	int q = 0;
	int r = 0;

	HexCoord() = default;
	HexCoord(int q_, int r_) : q(q_), r(r_) {}

	bool operator==(const HexCoord& other) const
	{
		return q == other.q && r == other.r;
	}

	bool operator!=(const HexCoord& other) const
	{
		return !(*this == other);
	}

	/// <summary>
	/// もう1つの座標との距離（ヘックス何マス分離れているか）を計算する。
	/// </summary>
	int Distance(const HexCoord& other) const
	{
		int dq = q - other.q;
		int dr = r - other.r;
		// axial座標での距離公式。
		return (std::abs(dq) + std::abs(dr) + std::abs(dq + dr)) / 2;
	}

	/// <summary>
	/// 隣接する6マスの座標を返す(移動時の1歩先候補の列挙に使う)。
	/// </summary>
	std::array<HexCoord, 6> GetNeighbors() const
	{
		static const int dirs[6][2] = { {1,0}, {1,-1}, {0,-1}, {-1,0}, {-1,1}, {0,1} };

		std::array<HexCoord, 6> result;
		for (int i = 0; i < 6; ++i)
		{
			result[i] = HexCoord(q + dirs[i][0], r + dirs[i][1]);
		}
		return result;
	}

	/// <summary>
	/// axial座標(q,r)をワールド座標に変換する(pointy-top、原点(0,0)がワールド原点)。
	/// 盤面全体のオフセットやマスの向きなどのレイアウト知識は呼び出し側の責務とする。
	/// </summary>
	/// <param name="hexSize">1マスのサイズ(中心から辺までの距離に相当するスケール)。</param>
	Vector3 ToWorldPosition(float hexSize) const
	{
		float worldX = hexSize * (sqrtf(3.0f) * q + sqrtf(3.0f) * 0.5f * r);
		float worldZ = hexSize * (1.5f * r);
		return Vector3(worldX, 0.0f, worldZ);
	}
};