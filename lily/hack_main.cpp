#include "hack.h"
#include <map>

#include "GNames.h"
#include "GObjects.h"

#include "pubg_class.h"

#include "info_bone.h"
#include "info_item.h"
#include "info_vehicle.h"
#include "info_proj.h"
#include "info_package.h"
#include "info_character.h"

void Hack::Loop(Process& process) {
	HWND hGameWnd = process.GetHwnd();
	CR3 mapCR3 = handler.GetMappedProcessCR3();
	Xenuine xenuine(process);
	TNameEntryArray NameArr;
	//FUObjectArray ObjectArr;
	//NameArr.DumpAllNames();
	//ObjectArr.DumpObject(NameArr);

	dbvm.CloakReset();

	uint8_t OriginalAimFuncBytes[0x24];
	AimHookAddress = 0;
	constexpr uintptr_t HookBaseAddress = 0x12c776f;
	if (process.GetBaseValueWithSize(HookBaseAddress, OriginalAimFuncBytes, sizeof(OriginalAimFuncBytes)))
		AimHookAddress = process.GetBaseAddress() + HookBaseAddress;

	int PrevMouseX = 0, PrevMouseY = 0;

	ObjectPtr<ATslCharacter> CachedMyTslCharacterPtr = 0;
	ObjectPtr<ATslCharacter> PrevTargetPtr = 0;

	while (1) {
		if (bTerminate || !IsWindow(hGameWnd))
			break;

		UpdateClientRect();
		float TimeDelta = ProcessTimeDelta();

		if (GetForegroundWindow() == hGameWnd) {
			InsertMouseInfo();
			ProcessHotkey();
		}

		const auto FuncInRenderArea = [&]() {
			DrawFPS(FPS, COLOR_TEAL);

			ProcessImGui();
			DrawHotkey();

			bool bAimHooked = false;

			[&] {
				TArray<ObjectPtr<AActor>> Actors;
				ObjectPtr<APlayerController> PlayerContollerPtr = 0;

				Vector CameraLocation(0.0f, 0.0f, 0.0f);
				Rotator CameraRotation(0.0f, 0.0f, 0.0f);
				float DefaultFOV = 90.0f;
				float CameraFOV = DefaultFOV;

				float InputPitchScale = 1.0f;
				float InputYawScale = 1.0f;
				float InputRollScale = 1.0f;
				constexpr float BallisticDragScale = 1.0f;
				constexpr float BallisticDropScale = 1.0f;

				ObjectPtr<UObject> MyPawnPtr = 0;

				[&] {
					UWorld World;
					if (!UWorld::GetUWorld(World))
						return;

					ULevel Level;
					if (!World.CurrentLevel.Read(Level))
						return;

					if (!Level.Actors.Read(Actors))
						return;

					UGameInstance GameInstance;
					if (!World.GameInstance.Read(GameInstance))
						return;

					EncryptedObjectPtr<ULocalPlayer> LocalPlayerPtr;
					if (!GameInstance.LocalPlayers.GetValue(0, LocalPlayerPtr))
						return;

					ULocalPlayer LocalPlayer;
					if (!LocalPlayerPtr.Read(LocalPlayer))
						return;

					PlayerContollerPtr = (uintptr_t)LocalPlayer.PlayerController;

					APlayerController PlayerController;
					if (!LocalPlayer.PlayerController.Read(PlayerController))
						return;

					InputPitchScale = PlayerController.InputPitchScale;
					InputYawScale = PlayerController.InputYawScale;
					InputRollScale = PlayerController.InputRollScale;

					if (PlayerController.Character)
						MyPawnPtr = (uintptr_t)PlayerController.Character;
					else if (CachedMyTslCharacterPtr)
						MyPawnPtr = (uintptr_t)CachedMyTslCharacterPtr;
					else if (PlayerController.SpectatorPawn)
						MyPawnPtr = (uintptr_t)PlayerController.SpectatorPawn;
					else
						MyPawnPtr = (uintptr_t)PlayerController.Pawn;
					CachedMyTslCharacterPtr = 0;

					APlayerCameraManager PlayerCameraManager;
					if (!PlayerController.PlayerCameraManager.Read(PlayerCameraManager))
						return;

					CameraLocation = PlayerCameraManager.CameraCache_POV_Location;
					CameraRotation = PlayerCameraManager.CameraCache_POV_Rotation;
					CameraFOV = PlayerCameraManager.CameraCache_POV_FOV;
					DefaultFOV = PlayerCameraManager.DefaultFOV;
				}();
				////////////////////////////////////////////////////////////////////////////////////////////////////////
				////////////////////////////////////////////////////////////////////////////////////////////////////////
				Vector MyLocation = CameraLocation;
				Matrix CameraRotationMatrix = Rotator(CameraRotation).GetMatrix(Vector(0.0, 0.0, 0.0));

				Vector GunLocation = MyLocation;
				Rotator GunRotation = CameraRotation;

				if (IsNearlyZero(CameraFOV))
					return;

				const float CircleFov = ConvertToRadians(CircleFovInDegrees);
				float AimbotCircleSize = tanf(CircleFov) * Height * (DefaultFOV / CameraFOV);
				if (CameraFOV < 40.0f)
					AimbotCircleSize /= 2;

				if (nAimbot >= 2)
					AimbotCircleSize = tanf(CircleFov * 1.5f) * Height * (DefaultFOV / CameraFOV);

				float AimbotDistant = AimbotCircleSize;

				auto DrawRatioBoxWrapper = [&](const Vector& ScreenPos, float CameraDistance, float BarLength3D, float Ratio, ImU32 ColorRemain, ImU32 ColorDamaged, ImU32 ColorEdge) {
					Vector ZeroLocation = Vector(0.0f, 0.0f, 0.0f);
					Matrix ZeroRotationMatrix = Rotator(0.0f, 0.0f, 0.0f).GetMatrix(Vector(0.0, 0.0, 0.0));
					Vector ScreenPos1 = WorldToScreen(Vector(CameraDistance, BarLength3D / 2.0f, 0.0f), ZeroRotationMatrix, ZeroLocation, CameraFOV);
					Vector ScreenPos2 = WorldToScreen(Vector(CameraDistance, -BarLength3D / 2.0f, 0.0f), ZeroRotationMatrix, ZeroLocation, CameraFOV);

					float ScreenLengthX = ScreenPos1.X - ScreenPos2.X;
					float ScreenLengthY = ScreenLengthX / 6.0f;

					ScreenLengthX = std::min(ScreenLengthX, Width / 16.0f);
					ScreenLengthX = std::max(ScreenLengthX, Width / 64.0f);

					ScreenLengthY = std::min(ScreenLengthY, 6.0f);
					ScreenLengthY = std::max(ScreenLengthY, 5.0f);

					DrawRatioBox(
						{ ScreenPos.X - ScreenLengthX / 2.0f, ScreenPos.Y - ScreenLengthY / 2.0f, ScreenPos.Z },
						{ ScreenPos.X + ScreenLengthX / 2.0f, ScreenPos.Y + ScreenLengthY / 2.0f, ScreenPos.Z },
						Ratio, ColorRemain, ColorDamaged, ColorEdge);

					return std::pair(ScreenLengthX, ScreenLengthY);
				};
				////////////////////////////////////////////////////////////////////////////////////////////////////////
				////////////////////////////////////////////////////////////////////////////////////////////////////////
				const unsigned MyCharacterNameHash = NameArr.GetNameHashByObject(MyPawnPtr);
				if (!MyCharacterNameHash)
					return;

				///////////////////////////////////////////////////////////
				int MyTeamNum = -1;
				int SpectatedCount = 0;
				bool IsWeaponed = false;
				bool IsScoping = false;

				float ZeroingDistance = FLT_MIN;
				ObjectPtr<UCurveVector> BallisticCurve = 0;
				float Gravity = -9.8f;
				float BDS = 1.0f;
				float VDragCoefficient = 1.0f;
				float SimulationSubstepTime = 0.016f;
				float InitialSpeed = 800.0f;
				float BulletDropAdd = 7.0f;

				uint8_t bUpdated = 0;

				//MyCharacterInfo
				[&] {
					if (!IsPlayerCharacter(MyCharacterNameHash))
						return;

					ATslCharacter MyTslCharacter;
					if (!MyPawnPtr.ReadOtherType(MyTslCharacter))
						return;

					CachedMyTslCharacterPtr = (uintptr_t)MyPawnPtr;
					MyTeamNum = MyTslCharacter.LastTeamNum;
					SpectatedCount = MyTslCharacter.SpectatedCount;

					if (MyTslCharacter.Health <= 0.0f)
						return;

					//MyCharacter Mesh Info
					[&] {
						USkeletalMeshComponent Mesh;
						if (!MyTslCharacter.Mesh.Read(Mesh))
							return;

						GunLocation = MyLocation = Mesh.ComponentToWorld.Translation;

						UTslAnimInstance TslAnimInstance;
						if (!Mesh.AnimScriptInstance.ReadOtherType(TslAnimInstance))
							return;

						IsScoping = TslAnimInstance.bIsScoping_CP;
						Rotator Recoil_CP = TslAnimInstance.RecoilADSRotation_CP;
						Recoil_CP.Yaw += (TslAnimInstance.LeanRightAlpha_CP - TslAnimInstance.LeanLeftAlpha_CP) * Recoil_CP.Pitch / 3.0f;
						GunRotation = Rotator(TslAnimInstance.ControlRotation_CP) + Recoil_CP;
					}();

					//MyCharacter Weapon Info
					[&] {
						ATslWeapon_Trajectory TslWeapon;
						if (!MyTslCharacter.GetTslWeapon(TslWeapon))
							return;

						IsWeaponed = true;
						Gravity = TslWeapon.TrajectoryGravityZ;
						if (IsScoping)
							ZeroingDistance = TslWeapon.GetZeroingDistance();

						UWeaponTrajectoryData WeaponTrajectoryData;
						if (TslWeapon.WeaponTrajectoryData.Read(WeaponTrajectoryData)) {
							BDS = WeaponTrajectoryData.TrajectoryConfig.BDS;
							VDragCoefficient = WeaponTrajectoryData.TrajectoryConfig.VDragCoefficient;
							SimulationSubstepTime = WeaponTrajectoryData.TrajectoryConfig.SimulationSubstepTime;
							BallisticCurve = WeaponTrajectoryData.TrajectoryConfig.BallisticCurve;
							InitialSpeed = WeaponTrajectoryData.TrajectoryConfig.InitialSpeed;
						}

						UWeaponMeshComponent WeaponMesh;
						if (TslWeapon.Mesh3P.Read(WeaponMesh)) {
							Transform GunTransform = WeaponMesh.GetSocketTransform(TslWeapon.FiringAttachPoint, RTS_World);

							GunLocation = GunTransform.Translation;
							if (IsScoping)
								GunRotation = GunTransform.Rotation;

							BulletDropAdd = WeaponMesh.GetScopingAttachPointRelativeZ(TslWeapon.ScopingAttachPoint) -
								GunTransform.GetRelativeTransform(WeaponMesh.ComponentToWorld).Translation.Z;
						}
					}();
				}();

				//Check CurrentTargetPtr is valid
				[&] {
					if (!bPushingMouseM) {
						PrevTargetPtr = 0;
						return;
					}

					if (!CachedMyTslCharacterPtr) {
						PrevTargetPtr = 0;
						return;
					}

					ATslCharacter TslCharacter;
					if (!PrevTargetPtr.Read(TslCharacter)) {
						PrevTargetPtr = 0;
						return;
					}

					if (TslCharacter.Health > 0.0f)
						return;

					if (TslCharacter.GroggyHealth <= 0.0f || !bPushingCTRL) {
						PrevTargetPtr = 0;
						return;
					}
				}();

				ObjectPtr<ATslCharacter> CurrentTargetPtr = 0;
				bool bFoundTarget = false;
				Vector TargetPos(0.0f, 0.0f, 0.0f);
				Vector TargetVelocity(0.0f, 0.0f, 0.0f);

				//Actor loop
				for (const auto& ActorPtr : Actors.GetVector()) {
					TSet<ObjectPtr<UActorComponent>> OwnedComponents;
					Vector ActorVelocity(0.0f, 0.0f, 0.0f);
					Vector ActorLocation(0.0f, 0.0f, 0.0f);
					Vector ActorLocationScreen(0.0f, 0.0f, 0.0f);
					float DistanceToActor = 0.0f;
					unsigned ActorNameHash = 0;

					auto GetValidActorInfo = [&]() {
						AActor Actor;
						if (!ActorPtr.Read(Actor))
							return false;

						OwnedComponents = Actor.OwnedComponents;

						USceneComponent RootComponent;
						if (!Actor.RootComponent.Read(RootComponent))
							return false;

						ActorVelocity = RootComponent.ComponentVelocity;
						ActorLocation = RootComponent.ComponentToWorld.Translation;
						ActorLocationScreen = WorldToScreen(ActorLocation, CameraRotationMatrix, CameraLocation, CameraFOV);
						DistanceToActor = MyLocation.Distance(ActorLocation) / 100.0f;

						if (nRange != 1000 && DistanceToActor > nRange)
							return false;

						USceneComponent AttachParent;
						//LocalPawn is vehicle(Localplayer is in vehicle)
						if (RootComponent.AttachParent.Read(AttachParent) && AttachParent.Owner == MyPawnPtr)
							return false;

						if (!NameArr.GetNameByID(Actor.GetFName(), szBuf, nBufSize))
							return false;

						ActorNameHash = CompileTime::Hash(szBuf);

						if (bDebug) {
							Vector v2_DebugLoc = ActorLocationScreen;
							v2_DebugLoc.Y += 15.0;
							DrawString(v2_DebugLoc, szBuf, COLOR_WHITE, false);
						}

						return true;
					};
					if (!GetValidActorInfo())
						continue;

					//DrawProj
					[&] {
						if (DistanceToActor > 300.0f)
							return;

						GetProjName(ActorNameHash, szBuf);
						if (strlen(szBuf) == 0)
							return;

						sprintf(szBuf, "%s\n%.0fM"e, szBuf, DistanceToActor);
						DrawString(ActorLocationScreen, szBuf, COLOR_RED, false);
					}();

					//DrawVehicle
					[&] {
						if (!bVehicle || bFighterMode)
							return;

						if (ActorNameHash == "BP_VehicleDrop_BRDM_C"h) {
							sprintf(szBuf, "BRDM\n%.0fM"e, DistanceToActor);
							DrawString(ActorLocationScreen, szBuf, COLOR_TEAL, false);
							return;
						}

						auto VehicleInfo = GetVehicleInfo(ActorNameHash, szBuf);
						if (strlen(szBuf) == 0)
							return;

						float Health = 100.0f;
						float HealthMax = 100.0f;
						float Fuel = 100.0f;
						float FuelMax = 100.0f;

						switch (std::get<0>(VehicleInfo)) {
						case VehicleType1::Wheeled:
						{
							ATslWheeledVehicle WheeledVehicle;
							if (!ActorPtr.ReadOtherType(WheeledVehicle))
								break;

							UTslVehicleCommonComponent VehicleComponent;
							if (!WheeledVehicle.VehicleCommonComponent.Read(VehicleComponent))
								break;

							Health = VehicleComponent.Health;
							HealthMax = VehicleComponent.HealthMax;
							Fuel = VehicleComponent.Fuel;
							FuelMax = VehicleComponent.FuelMax;
							break;
						}
						case VehicleType1::Floating:
						{
							ATslFloatingVehicle WheeledVehicle;
							if (!ActorPtr.ReadOtherType(WheeledVehicle))
								break;

							UTslVehicleCommonComponent VehicleComponent;
							if (!WheeledVehicle.VehicleCommonComponent.Read(VehicleComponent))
								break;

							Health = VehicleComponent.Health;
							HealthMax = VehicleComponent.HealthMax;
							Fuel = VehicleComponent.Fuel;
							FuelMax = VehicleComponent.FuelMax;
							break;
						}
						}

						if (ActorNameHash == "BP_LootTruck_C"h && Health <= 0.0f)
							return;

						bool IsDestructible = (std::get<1>(VehicleInfo) == VehicleType2::Destructible);

						ImU32 Color = COLOR_BLUE;
						if (std::get<2>(VehicleInfo) == VehicleType3::Special)
							Color = COLOR_TEAL;
						if (Health <= 0.0f || Fuel <= 0.0f)
							Color = COLOR_GRAY;

						sprintf(szBuf, "%s\n%.0fM"e, szBuf, DistanceToActor);
						DrawString(ActorLocationScreen, szBuf, Color, false);

						//Draw vehicle health, fuel
						[&] {
							if (!IsDestructible)
								return;

							Vector VehicleBarScreenPos = ActorLocationScreen;
							VehicleBarScreenPos.Y += GetTextSize(szBuf).y / 2.0f + 4.0f;
							const float CameraDistance = CameraLocation.Distance(ActorLocation) / 100.0f;
							if (Health <= 0.0f)
								return;
					
							float HealthBarScreenLengthY = DrawRatioBoxWrapper(VehicleBarScreenPos, CameraDistance, 1.0f, Health / HealthMax, COLOR_GREEN, COLOR_RED, COLOR_BLACK).second;
							VehicleBarScreenPos.Y += HealthBarScreenLengthY - 1.0f;
							DrawRatioBoxWrapper(VehicleBarScreenPos, CameraDistance, 1.0f, Fuel / FuelMax, COLOR_BLUE, COLOR_GRAY, COLOR_BLACK).second;
						}();
					}();
					
					auto DrawItem = [&](ObjectPtr<UItem> ItemPtr, Vector ItemLocation) {
						UItem Item;
						if (!ItemPtr.Read(Item))
							return false;

						unsigned ItemHash = NameArr.GetNameHashByID(Item.GetItemID());
						if (!ItemHash)
							return false;

						int ItemPriority = std::get<0>(GetItemInfo(ItemHash, szBuf));
						ImU32 Color = GetItemColor(ItemPriority);

						if (ItemPriority >= nItem)
							DrawString(ItemLocation, szBuf, Color, false);
						else if (bDebug)
							DrawString(ItemLocation, szBuf, COLOR_WHITE, false);
						else
							return false;
						return true;
					};

					//DrawBox
					[&] {
						if (!bBox || bFighterMode)
							return;

						GetPackageName(ActorNameHash, szBuf);
						if (strlen(szBuf) == 0)
							return;

						sprintf(szBuf, "%s\n%.0fM"e, szBuf, DistanceToActor);
						DrawString(ActorLocationScreen, szBuf, COLOR_TEAL, false);

						//DrawBoxContents
						[&] {
							if (!bPushingMouseM || nItem == 0)
								return;

							AItemPackage ItemPackage;
							if (!ActorPtr.ReadOtherType(ItemPackage))
								return;

							Vector PackageLocationScreen = ActorLocationScreen;

							const float TextLineHeight = GetTextSize(""e).y;
							PackageLocationScreen.Y += TextLineHeight * 1.5f;

							for (const auto& ItemPtr : ItemPackage.Items.GetVector()) {
								if (!DrawItem(ItemPtr, PackageLocationScreen))
									continue;
								PackageLocationScreen.Y += TextLineHeight - 1.0f;
							}
						}();
					}();

					//DrawDropptedItem
					[&] {
						if (nItem == 0 || bFighterMode || ActorNameHash != "DroppedItem"h)
							return;

						ADroppedItem DroppedItem;
						if (!ActorPtr.ReadOtherType(DroppedItem))
							return;

						DrawItem((uintptr_t)DroppedItem.Item, ActorLocationScreen);
					}();

					//DrawDroppedItemGroup
					[&] {
						if (nItem == 0 || bFighterMode || ActorNameHash != "DroppedItemGroup"h)
							return;

						for (const auto& Element : OwnedComponents.GetVector()) {
							UDroppedItemInteractionComponent ItemComponent;
							if (!Element.Value.ReadOtherType(ItemComponent))
								continue;

							unsigned ItemComponentHash = NameArr.GetNameHashByID(ItemComponent.GetFName());
							if (!ItemComponentHash)
								continue;

							if (ItemComponentHash != "DroppedItemInteractionComponent"h && ItemComponentHash != "DestructibleItemInteractionComponent"h)
								continue;

							Vector ItemLocationScreen = WorldToScreen(ItemComponent.ComponentToWorld.Translation, CameraRotationMatrix, CameraLocation, CameraFOV);
							DrawItem(ItemComponent.Item, ItemLocationScreen);
						}
					}();

					//Character
					[&] {
						if (!IsPlayerCharacter(ActorNameHash) && !IsAICharacter(ActorNameHash))
							return;
						
						if (MyPawnPtr == ActorPtr)
							return;

						ATslCharacter TslCharacter;
						if (!ActorPtr.ReadOtherType(TslCharacter))
							return;

						float Health = TslCharacter.Health;
						float HealthMax = TslCharacter.HealthMax;
						float GroggyHealth = TslCharacter.GroggyHealth;
						float GroggyHealthMax = TslCharacter.GroggyHealthMax;

						if (Health <= 0.0f && GroggyHealth <= 0.0f)
							return;

						int NumKills = 0;
						float DamageDealtOnEnemy = 0.0f;
						bool IsInVehicle = false;

						//GetPlayerState
						[&] {
							if (!TslCharacter.PlayerState)
								return;

							ATslPlayerState TslPlayerState;
							if (!TslCharacter.PlayerState.ReadOtherType(TslPlayerState))
								return;

							NumKills = TslPlayerState.PlayerStatistics_NumKills;
							DamageDealtOnEnemy = TslPlayerState.DamageDealtOnEnemy;
						}();
						
						//GetPlayerVehicleInfo
						[&] {
							UVehicleRiderComponent VehicleRiderComponent;
							if (!TslCharacter.VehicleRiderComponent.Read(VehicleRiderComponent))
								return;

							if (VehicleRiderComponent.SeatIndex == -1)
								return;

							APawn LastVehiclePawn;
							if (!VehicleRiderComponent.LastVehiclePawn.Read(LastVehiclePawn))
								return;

							IsInVehicle = true;
							ActorVelocity = LastVehiclePawn.ReplicatedMovement.LinearVelocity;

							ATslPlayerState TslPlayerState;
							if (!LastVehiclePawn.PlayerState.ReadOtherType(TslPlayerState))
								return;

							NumKills = TslPlayerState.PlayerStatistics_NumKills;
							DamageDealtOnEnemy = TslPlayerState.DamageDealtOnEnemy;
						}();

						//DrawRadar
						[&] {
							if (TslCharacter.LastTeamNum == MyTeamNum)
								return;

							Vector ALfromME = ActorLocation - MyLocation;
							int RadarX = (int)round(ALfromME.X / 100);
							int RadarY = (int)round(ALfromME.Y / 100);

							//when ememy in the radar radius.
							if (RadarX > 200 || RadarX < -200 || RadarY > 200 || RadarY < -200)
								return; 
							
							Vector v;
							v.X = (Width * (0.9807f + 0.8474f) / 2.0f) + (RadarX / 200.0f * Width * (0.9807f - 0.8474f) / 2.0f);
							v.Y = (Height * (0.9722f + 0.7361f) / 2.0f) + (RadarY / 200.0f * Height * (0.9722f - 0.7361f) / 2.0f);

							ImU32 Color = COLOR_RED;

							if (IsInVehicle)
								Color = COLOR_BLUE;
							if (GroggyHealth > 0.0f && Health <= 0.0f)
								Color = COLOR_GRAY;

							DrawRectFilled({ v.X - 3.0f, v.Y - 3.0f, 0.0 }, { v.X + 3.0f, v.Y + 3.0f, 0.0f }, Color);
						}();

						//DrawSkeleton, Aimbot
						[&] {
							USkeletalMeshComponent Mesh;
							if (!TslCharacter.Mesh.Read(Mesh))
								return;

							auto BoneSpaceTransforms = Mesh.BoneSpaceTransforms.GetVector(0x200);
							size_t BoneSpaceTransformsSize = BoneSpaceTransforms.size();
							if (!BoneSpaceTransformsSize)
								return;

							Transform ComponentToWorld = Mesh.ComponentToWorld;
							std::map<int, Vector> BonesPos, BonesScreenPos;

							//GetBonesPosition
							for (auto BoneIndex : GetBoneIndexArray()) {
								verify(BoneIndex < BoneSpaceTransformsSize);
								Vector Pos = ((Transform)BoneSpaceTransforms[BoneIndex] * ComponentToWorld).Translation;
								BonesPos[BoneIndex] = Pos;
								BonesScreenPos[BoneIndex] = WorldToScreen(Pos, CameraRotationMatrix, CameraLocation, CameraFOV);
							}

							Vector HealthBarPos = BonesPos[neck_01];
							HealthBarPos.Z += 35.0f;
							Vector HealthBarScreenPos = WorldToScreen(HealthBarPos, CameraRotationMatrix, CameraLocation, CameraFOV);
							HealthBarScreenPos.Y -= 5.0f;

							const float CameraDistance = CameraLocation.Distance(HealthBarPos) / 100.0f;
							float HealthBarScreenLengthY = 0.0f;
							if (Health > 0.0)
								HealthBarScreenLengthY = DrawRatioBoxWrapper(HealthBarScreenPos, CameraDistance, 0.7f, Health / HealthMax, COLOR_GREEN, COLOR_RED, COLOR_BLACK).second;
							else if (GroggyHealth > 0.0)
								HealthBarScreenLengthY = DrawRatioBoxWrapper(HealthBarScreenPos, CameraDistance, 0.7f, GroggyHealth / GroggyHealthMax, COLOR_RED, COLOR_GRAY, COLOR_BLACK).second;

							bool IsInCircle = false;
							//Aimbot
							[&] {
								if (!IsWeaponed)
									return;
								if (!Mesh.IsVisible())
									return;
								if (!bTeamKill && TslCharacter.LastTeamNum == MyTeamNum)
									return;
								if (!bPushingCTRL && Health <= 0.0f)
									return;

								Vector VecEnemy = (BonesPos[neck_01] + BonesPos[spine_02]) * 0.5f;
								if (bPushingShift)
									VecEnemy = BonesPos[forehead];

								Vector VecEnemy2D = WorldToScreen(VecEnemy, CameraRotationMatrix, CameraLocation, CameraFOV);
								if (VecEnemy2D.Z < 0.0f)
									return;

								VecEnemy2D.Z = 0.0f;
								Vector Center2D = { Width / 2.0f, Height / 2.0f, 0 };

								float DistanceFromCenter = Center2D.Distance(VecEnemy2D);

								if (DistanceFromCenter < AimbotCircleSize)
									IsInCircle = true;

								if (DistanceFromCenter > AimbotDistant)
									return;

								if (PrevTargetPtr && PrevTargetPtr != ActorPtr)
									return;

								bFoundTarget = true;
								CurrentTargetPtr = (uintptr_t)ActorPtr;
								AimbotDistant = DistanceFromCenter;
								TargetPos = VecEnemy;
								TargetVelocity = ActorVelocity;
							}();

							//DrawCharacter
							[&] {
								//GetColor
								ImU32 Color = [&] {
									if (ActorPtr == PrevTargetPtr)
										return COLOR_RED;
									if (TslCharacter.LastTeamNum == MyTeamNum)
										return COLOR_GREEN;
									if (IsInCircle)
										return COLOR_YELLOW;
									if (IsInVehicle)
										return COLOR_BLUE;
									if (Health <= 0.0f)
										return COLOR_GRAY;
									if (IsAICharacter(ActorNameHash))
										return COLOR_TEAL;
									return (DamageDealtOnEnemy > 510.0) ? COLOR_RED : IM_COL32(255, 255 - int(DamageDealtOnEnemy / 2), 255 - int(DamageDealtOnEnemy / 2), 255);
								}();

								for (auto DrawPair : GetDrawPairArray())
									DrawLine(BonesScreenPos[DrawPair.first], BonesScreenPos[DrawPair.second], Color);

								wchar_t PlayerName[0x100];
								TslCharacter.CharacterName.GetValues(*PlayerName, 0x100);

								if (TslCharacter.LastTeamNum >= 200 || TslCharacter.LastTeamNum == MyTeamNum) {
									sprintf(szBuf, "%ws %d\n%.0fM %.0f"e, PlayerName, NumKills, DistanceToActor, DamageDealtOnEnemy);
								}
								else {
									sprintf(szBuf, "%ws %d\n%.0fM %d %.0f"e, PlayerName, NumKills, DistanceToActor, TslCharacter.LastTeamNum, DamageDealtOnEnemy);
								}

								DrawString(
									{ HealthBarScreenPos.X, HealthBarScreenPos.Y - HealthBarScreenLengthY - GetTextSize(szBuf).y / 2.0f, HealthBarScreenPos.Z },
									szBuf, Color, true);
							}();
						}();
					}();
				}

				float CustomTimeDilation = 1.0f;

				//TargetCharacter
				[&] {
					if (!bFoundTarget)
						return;

					if (bPushingMouseM && !PrevTargetPtr)
						PrevTargetPtr = (uintptr_t)CurrentTargetPtr;

					if (nAimbot == 2 || nAimbot == 3 || !IsScoping)
						ZeroingDistance = FLT_MIN;

					auto Result = GetBulletDropAndTravelTime(
						GunLocation, GunRotation, TargetPos, ZeroingDistance, BulletDropAdd, InitialSpeed,
						Gravity, BallisticDragScale, BallisticDropScale, BDS, SimulationSubstepTime, VDragCoefficient, BallisticCurve);

					float BulletDrop = Result.first;
					float TravelTime = Result.second;
					
					Vector PredictedPos = TargetPos;
					PredictedPos.Z += BulletDrop;
					PredictedPos = PredictedPos + (TargetVelocity * (TravelTime / CustomTimeDilation));

					Vector TargetScreenPos = WorldToScreen(TargetPos, CameraRotationMatrix, CameraLocation, CameraFOV);
					Vector AimScreenPos = WorldToScreen(PredictedPos, CameraRotationMatrix, CameraLocation, CameraFOV);
					DrawLine(TargetScreenPos, AimScreenPos, COLOR_RED);

					if (!IsWeaponed || !bPushingMouseM)
						return;

					auto Aimbot_RotationInput = [&] {
						if (bTurnBack)
							return;

						Rotator RotationInput = (PredictedPos - CameraLocation).GetDirectionRotator() - GunRotation;
						RotationInput.Clamp();
						RotationInput.Pitch = RotationInput.Pitch / -InputPitchScale;
						RotationInput.Yaw = RotationInput.Yaw / InputYawScale;
						RotationInput.Roll = 0.0;

						PlayerContollerPtr.WriteAtOffset(RotationInput, offsetof(APlayerController, RotationInput));
					};

					auto AImbot_MouseEvent = [&] {
						if (hGameWnd != GetForegroundWindow())
							return;

						const Vector Loc = WorldToScreen(PredictedPos, GunRotation.GetMatrix(Vector(0.0, 0.0, 0.0)), CameraLocation, CameraFOV);
						const float ScreenCenterX = Width / 2.0f;
						const float ScreenCenterY = Height / 2.0f;
						const int MouseX = int((Loc.X - ScreenCenterX) / (Width / 1280.0f));
						const int MouseY = int((Loc.Y - ScreenCenterY) / (Height / 720.0f)) + 1;
						if (PrevMouseX == MouseX && PrevMouseY == MouseY)
							return;

						mouse_event(MOUSEEVENTF_MOVE, MouseX, MouseY, 0, 0);
						PrevMouseX = MouseX;
						PrevMouseY = MouseY;
					};

					if (nAimbot == 1)
						Aimbot_RotationInput();
					else if(nAimbot == 2 && !IsScoping)
						Aimbot_RotationInput();
					else if (AimHookAddress && (nAimbot == 2 || nAimbot == 3)) {
						Vector DirectionInput = PredictedPos - GunLocation;
						DirectionInput.Normalize();

						uint8_t HookedFunction[sizeof(OriginalAimFuncBytes)] = {
							0xC7, 0x84, 0x24, 0x08, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
							0xC7, 0x84, 0x24, 0x0C, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
							0xC7, 0x84, 0x24, 0x10, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
							0x90, 0x90, 0x90
						};

						*(float*)(HookedFunction + 7) = DirectionInput.X;
						*(float*)(HookedFunction + 7 + 11) = DirectionInput.Y;
						*(float*)(HookedFunction + 7 + 22) = DirectionInput.Z;
						dbvm.WPMCloak(AimHookAddress, HookedFunction, sizeof(HookedFunction), mapCR3);
						bAimHooked = true;
					}
				}();

				if (SpectatedCount > 0)
					DrawSpectatedCount(SpectatedCount, COLOR_RED);
				if(!IsNearlyZero(ZeroingDistance))
					DrawZeroingDistance(ZeroingDistance, COLOR_TEAL);

				if (IsWeaponed)
					DrawCircle({ Width / 2.0f, Height / 2.0f }, AimbotCircleSize, COLOR_WHITE);

				if (bTurnBack) {
					bTurnBack = false;
					Rotator RotationInput(0.0f, 180.0f, 0.0f);
					PlayerContollerPtr.WriteAtOffset(RotationInput, offsetof(APlayerController, RotationInput));
				}
			}();

			if (!bAimHooked) {
				dbvm.WPMCloak(AimHookAddress, OriginalAimFuncBytes, sizeof(OriginalAimFuncBytes), mapCR3);
			}
		};

		RenderArea(FuncInRenderArea);
	}

	Clear();
}

void FUObjectArray::DumpObject(const TNameEntryArray& NameArr) const {
	for (unsigned i = 0; i < GetNumElements(); i++) {
		UObject obj;
		auto Ptr = GetObjectPtrById(i);
		if (!Ptr.Read(obj))
			continue;

		char szName[0x200];
		if (!NameArr.GetNameByID(obj.GetFName(), szName, sizeof(szName)))
			continue;

		if (_stricmp(szName, "World"e) == 0) {
			dprintf("%I64X %s\n"e, (uintptr_t)Ptr, szName);
		}
		if (_stricmp(szName, "PlayerState"e) == 0) {
			dprintf("%I64X %s\n"e, (uintptr_t)Ptr, szName);
		}
		if (_stricmp(szName, "ForceLayoutPrepass"e) == 0) {
			dprintf("%I64X %s\n"e, (uintptr_t)Ptr, szName);
		}
	}
}