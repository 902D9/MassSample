﻿// Fill out your copyright notice in the Description page of Project Settings.

#include "MSOctreeProcessors.h"

#include "MassCommonFragments.h"
#include "MassCommonTypes.h"
#include "MassMovementFragments.h"
#include "Common/Fragments/MSOctreeFragments.h"


UMSOctreeProcessor::UMSOctreeProcessor()
{
	ExecutionOrder.ExecuteAfter.Add(UE::Mass::ProcessorGroupNames::Movement);
	ExecutionFlags = (int32)EProcessorExecutionFlags::All;
}

void UMSOctreeProcessor::Initialize(UObject& Owner)
{
	MassSampleSystem = GetWorld()->GetSubsystem<UMSSubsystem>();
}

void UMSOctreeProcessor::ConfigureQueries()
{
	// Ideally we only do this for meshes that actually moved

	UpdateOctreeQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	UpdateOctreeQuery.AddRequirement<FMSOctreeFragment>(EMassFragmentAccess::ReadWrite);
	UpdateOctreeQuery.AddTagRequirement<FMSInOctreeGridTag>(EMassFragmentPresence::All);
	UpdateOctreeQuery.RegisterWithProcessor(*this);
}

void UMSOctreeProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	FMSOctree2& Octree = MassSampleSystem->Octree2;

	UpdateOctreeQuery.ForEachEntityChunk(EntityManager, Context, [&](FMassExecutionContext& Context)
	{
		const int32 NumEntities = Context.GetNumEntities();

		const auto LocationList = Context.GetFragmentView<FTransformFragment>();
		const auto OctreeFragments = Context.GetMutableFragmentView<FMSOctreeFragment>();

		for (int32 i = 0; i < NumEntities; ++i)
		{
			const FVector& Location = LocationList[i].GetTransform().GetLocation();
			auto OctreeFragment = OctreeFragments[i];
			
			FOctreeElementId2 OctreeID = *OctreeFragment.OctreeID.Get();

			if(Octree.IsValidElementId(OctreeID))
			{
				FMSEntityOctreeElement TempOctreeElement = Octree.GetElementById(OctreeID);
				
				Octree.RemoveElement(OctreeID);
				
				// Set the new location... I think ?
				TempOctreeElement.Bounds.Center = FVector4(Location,0);

				//DrawDebugBox(EntityManager.GetWorld(), TempOctreeElement.Bounds.Center, TempOctreeElement.Bounds.Extent, FColor::White);

				
				Octree.AddElement(TempOctreeElement);
			}
			
		}
	});

	auto World = EntityManager.GetWorld();
}

UMSHashGridMemberInitializationProcessor::UMSHashGridMemberInitializationProcessor()
{
	ObservedType = FMSOctreeFragment::StaticStruct();
	bRequiresGameThreadExecution = true;
	Operation = EMassObservedOperation::Add;
}

void UMSHashGridMemberInitializationProcessor::Initialize(UObject& Owner)
{
	MassSampleSystem = GetWorld()->GetSubsystem<UMSSubsystem>();
}

void UMSHashGridMemberInitializationProcessor::ConfigureQueries()
{
	EntityQuery.AddRequirement<FMSOctreeFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.RegisterWithProcessor(*this);
}

void UMSHashGridMemberInitializationProcessor::Execute(FMassEntityManager& EntityManager,
                                                       FMassExecutionContext& Context)
{
	EntityQuery.ForEachEntityChunk(EntityManager, Context, [&](FMassExecutionContext& Context)
	{
		const auto LocationList = Context.GetFragmentView<FTransformFragment>();
		const auto NavigationObstacleCellLocationList = Context.GetMutableFragmentView<FMSOctreeFragment>();

		const int32 NumEntities = Context.GetNumEntities();

		for (int32 i = 0; i < NumEntities; ++i)
		{
			const FVector& Location = LocationList[i].GetTransform().GetLocation();

			auto& Octree = MassSampleSystem->Octree2;


			
			auto Entity = Context.GetEntity(i);

			
			FMSEntityOctreeElement NewOctreeElement;
			NewOctreeElement.Bounds = FBoxCenterAndExtent(Location, FVector(100.0f));
			NewOctreeElement.SharedOctreeID = MakeShared<FOctreeElementId2,ESPMode::ThreadSafe>() ;
			NewOctreeElement.EntityHandle = Entity;

			Octree.AddElement(NewOctreeElement);
			
			NavigationObstacleCellLocationList[i].OctreeID = NewOctreeElement.SharedOctreeID;
			Context.Defer().AddTag<FMSInOctreeGridTag>(Context.GetEntity(i));
		}
	});
}

UMSOctreeMemberCleanupProcessor::UMSOctreeMemberCleanupProcessor()
{
	ObservedType = FMSOctreeFragment::StaticStruct();
	bRequiresGameThreadExecution = true;
	Operation = EMassObservedOperation::Remove;
}

void UMSOctreeMemberCleanupProcessor::Initialize(UObject& Owner)
{
	MassSampleSystem = GetWorld()->GetSubsystem<UMSSubsystem>();
}

void UMSOctreeMemberCleanupProcessor::ConfigureQueries()
{
	EntityQuery.AddRequirement<FMSOctreeFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.RegisterWithProcessor(*this);}

void UMSOctreeMemberCleanupProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	EntityQuery.ForEachEntityChunk(EntityManager, Context, [&](FMassExecutionContext& Context)
	{
		const auto NavigationObstacleCellLocationList = Context.GetMutableFragmentView<FMSOctreeFragment>();

		const int32 NumEntities = Context.GetNumEntities();

		for (int32 i = 0; i < NumEntities; ++i)
		{
			auto& Octree = MassSampleSystem->Octree2;
			auto OctreeID = NavigationObstacleCellLocationList[i].OctreeID;
			Octree.RemoveElement(*OctreeID);
		}
	});
}
