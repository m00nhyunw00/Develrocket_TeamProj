#pragma once

#include "PCG/CityPacking.h"
#include "Math/RandomStream.h"

namespace CityPacking
{
    struct FRect
    {
        double X, Y, W, H;
        bool Contains(const FRect& B) const { return B.X >= X - 0.001 && B.Y >= Y - 0.001 && B.X + B.W <= X + W + 0.001 && B.Y + B.H <= Y + H + 0.001; }
        bool Intersects(const FRect& B) const { return X < B.X + B.W - 0.001 && X + W > B.X + 0.001 && Y < B.Y + B.H - 0.001 && Y + H > B.Y + 0.001; }
    };

    struct FPlacement { FRect Rect; int32 Type; int32 Quarter; };

    // 점유 사각형을 제외한 잔여 공간 분할 및 포함 사각형 제거
    inline void Occupy(TArray<FRect>& Free, const FRect& Used)
    {
        TArray<FRect> Next;
        for (const FRect& R : Free)
        {
            if (!R.Intersects(Used)) { Next.Add(R); continue; }
            if (Used.X > R.X) Next.Add({R.X, R.Y, Used.X - R.X, R.H});
            if (Used.X + Used.W < R.X + R.W) Next.Add({Used.X + Used.W, R.Y, R.X + R.W - Used.X - Used.W, R.H});
            if (Used.Y > R.Y) Next.Add({R.X, R.Y, R.W, Used.Y - R.Y});
            if (Used.Y + Used.H < R.Y + R.H) Next.Add({R.X, Used.Y + Used.H, R.W, R.Y + R.H - Used.Y - Used.H});
        }
        for (int32 I = Next.Num() - 1; I >= 0; --I)
        {
            bool Remove = Next[I].W < 1 || Next[I].H < 1;
            for (int32 J = 0; !Remove && J < Next.Num(); ++J)
                if (I != J && Next[J].Contains(Next[I])) Remove = true;
            if (Remove) Next.RemoveAt(I);
        }
        Free = MoveTemp(Next);
    }

    // 기존 전체 Floor 패킹과 블록 패킹의 공용 계산. Weights는 배치 점수의 상대 선호도
    inline TArray<FPlacement> Pack(const TArray<FCityBuildingFootprint>& Types, double W, double H,
        double Gap, int32 Limit, FRandomStream& Random, bool bAllowQuarterTurns = true,
        const TArray<double>* Weights = nullptr)
    {
        TArray<FRect> Free; Free.Add({0, 0, W + Gap, H + Gap});
        TArray<FPlacement> Result;
        TArray<int32> Counts; Counts.Init(0, Types.Num());
        for (int32 Step = 0; Step < Limit; ++Step)
        {
            struct FCandidate { FPlacement P; double Score; };
            TArray<FCandidate> Candidates;
            double LeastCount = MAX_int32;
            for (int32 T = 0; T < Types.Num(); ++T)
            {
                const FVector Size = Types[T].BoundsMax - Types[T].BoundsMin;
                if (!Types[T].BuildingClass || Size.X <= 0 || Size.Y <= 0) continue;
                const double Weight = Weights ? FMath::Max((*Weights)[T], 0.000001) : 1.0;
                for (int32 Q = 0; Q < (bAllowQuarterTurns ? 2 : 1); ++Q)
                {
                    const double BW = (Q ? Size.Y : Size.X) + Gap;
                    const double BH = (Q ? Size.X : Size.Y) + Gap;
                    for (const FRect& R : Free)
                    {
                        if (BW > R.W + 0.001 || BH > R.H + 0.001) continue;
                        LeastCount = FMath::Min(LeastCount, Counts[T] / Weight);
                        const double Short = FMath::Min(R.W - BW, R.H - BH);
                        const double Noise = Random.FRandRange(0, 800);
                        Candidates.Add({{{R.X, R.Y, BW, BH}, T, Q}, Short + Noise / Weight});
                    }
                }
            }
            if (Candidates.IsEmpty()) break;
            for (FCandidate& C : Candidates)
            {
                const double Weight = Weights ? FMath::Max((*Weights)[C.P.Type], 0.000001) : 1.0;
                C.Score += FMath::Max(0.0, Counts[C.P.Type] / Weight - LeastCount) * 2500;
            }
            Candidates.StableSort([](const FCandidate& A, const FCandidate& B) { return A.Score < B.Score; });
            FPlacement P = Candidates[0].P;
            P.Quarter += Random.RandRange(0, 1) * 2;
            Result.Add(P);
            ++Counts[P.Type];
            Occupy(Free, P.Rect);
        }
        return Result;
    }
}
