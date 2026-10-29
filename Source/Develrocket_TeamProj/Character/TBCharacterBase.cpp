// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/TBCharacterBase.h"

// Sets default values
ATBCharacterBase::ATBCharacterBase()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void ATBCharacterBase::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ATBCharacterBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

// Called to bind functionality to input
void ATBCharacterBase::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

}

