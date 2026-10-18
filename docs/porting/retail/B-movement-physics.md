Record — kept as history; the current state is in README.md, BUILD.md and the change inventory.

# B: collision, movement, physics and interpolation (Engine.dll): prototype vs retail

Scope: the pairs assigned in the task. Method: Ghidra decompiles were used to get oriented. Every claimed difference
was then checked against `objdump` of both DLLs, using a normalised diff (call targets resolved to export, import or
body-hash names; data constants resolved; stack-slot displacements masked). The normaliser is a scratch script and is
not part of the repo. The vtable slots used in the IsOverlapping reconstruction were read from the retail
`??_7UPrimitive@@6B@` vftable: +0x58 = PointCheck, +0x64 = GetCollisionExtent.

Summary of REAL changes (details and patches below):

| # | Function (our source) | Retail change | Confidence |
|---|---|---|---|
| 1 | `AActor::IsOverlapping` (UnActor.cpp:1108, AActor.h:71) | Rewritten. New signature `(AActor* Other, FLOAT Expand)`, non-const. It is now a primitive `PointCheck` of this actor's expanded collision extent against Other's primitive. The brush and Level exclusions were removed. | high |
| 2 | `ULevel::MoveActor` (UnLevAct.cpp:892) | Touch loop has a new filter `(TestAdjust+DeltaSize)*Test->Time < DeltaSize`; untouch uses `IsOverlapping(...,0.1f)`. | high |
| 3 | `ULevel::CheckEncroachment` (UnLevAct.cpp:1203) | untouch uses `IsOverlapping(...,0.5f)`. | high |
| 4 | `APawn::physWalking` (UnPhysic.cpp:714) | After the drop-to-floor `MoveActor`/`SetBase`, a new early `return` if `Physics==PHYS_Falling`. | high |
| 5 | `AInterpolationManager::performPhysics` (UnPhysic.cpp:2666) | `Dest->Prev` is replaced by `Last` (OldCoords, StartControlPoint, Smoothing). A debug `debugf` "IPSnitch ..." was added; it never fires. | high |
| 6 | `AActor::CheckHearSound` (UnScript.cpp:1126) | `SLOT_Talk` sounds skip the line-of-sight occlusion (no radius or volume reduction). | high |
| 7 | `FMovingBrushTracker::Update` (UnDynBsp.cpp:254) and the new `FMovingBrushTracker::FlushAllRelatedMovers` (retail-only 10394660) | When a single mover moves, other stationary movers whose first BSP node's zones are in the moving mover's node ZoneMask are added to the flush group. | high (logic); medium on the exact source placement |

Everything else in these pairs is layout-only or a compiler artifact. That covers field-offset shifts from
`AInterpolationManager.Last`, assert line numbers, SEH handler addresses, register allocation, and the `FVector`
constructor being inlined in one build and an out-of-line call in the other.

---

## 1. AActor::IsOverlapping — proto `103029c8`→body `1037f9f0`, retail `10303558`→body `103809c0`

**Source:** `Source/Engine/Src/UnActor.cpp:1108` and declaration `Source/Engine/Inc/AActor.h:71`. The proto body
matches our source exactly: both IsBrush checks, the `Other!=Level` check, then the cylinder test (+0x11c Location,
+0x1f0 CollisionRadius, +0x1f8 CollisionHeight, +0x90 Level).

**Retail export:** `?IsOverlapping@AActor@@QAEHPAV1@M@Z`, i.e. `int __thiscall AActor::IsOverlapping(AActor*, float)`.
It is non-const, and Other is non-const. The old export `?IsOverlapping@AActor@@QBEHPBV1@@Z` is gone. No other
retail DLL or EXE imports either name. In our tree the only callers are the two in UnLevAct.cpp.

**Exact retail body** (objdump 103809c0..10380a92, `ret 0x8`; no SEH frame, because guardSlow is compiled out):

```
sub esp,0x38 ; ebx=0
[esp+0x18..0x43] = FCheckResult Hit(1.f)      ; Next=0,Actor=0,Location=0,Normal=0,Primitive=0,Time=1.0f,Item=-1
eax = this->GetPrimitive()                    ; AActor vtbl+0xa4
eax->GetCollisionExtent(&tmp, this)           ; UPrimitive vtbl+0x64 -> FVector tmp
tmp.X += Expand; tmp.Y += Expand; tmp.Z += Expand   ; Expand = [esp+0x4c] = 2nd arg
eax = Other->GetPrimitive()
r = eax->PointCheck(Hit, Other, this->Location, tmp, 0)   ; UPrimitive vtbl+0x58
return r==0                                   ; neg/sbb/inc
```

So: the float is a uniform expansion added to all three components of this actor's own collision extent. The
primitive used is Other's (`Other->GetPrimitive()->PointCheck`) with Owner=Other, the point is `this->Location`, and
ExtraNodeFlags is 0. The brush, Level and cylinder special-casing is gone. A mover or BSP brush (UModel::PointCheck),
a mesh (CT_Shape) or a box primitive is now tested properly. Previously any brush meant "not overlapping", so a
brush-touch was dropped on every move. For two plain cylinders the result equals the old test with `Expand` slack on
radius and height, and with `Other->CollisionWidth` honoured as a Z offset, which is how `UPrimitive::PointCheck`
treats it.

**Patch (confidence: high):**

```diff
--- a/Source/Engine/Inc/AActor.h
+++ b/Source/Engine/Inc/AActor.h
@@ -68,7 +68,8 @@
 	// AActor collision functions.
 	virtual UPrimitive* GetPrimitive() const;
 	FVector GetCylinderExtent() const;
-	UBOOL IsOverlapping( const AActor *Other ) const;
+	// Retail 021009: ?IsOverlapping@AActor@@QAEHPAV1@M@Z (non-const, Expand added).
+	UBOOL IsOverlapping( AActor *Other, FLOAT Expand );
 
 	// AActor general functions.
 	void BeginTouch(AActor *Other);
```

```diff
--- a/Source/Engine/Src/UnActor.cpp
+++ b/Source/Engine/Src/UnActor.cpp
@@ -1105,27 +1105,20 @@
 //
 // Return whether this actor overlaps another.
+// Retail 021009: point-check this actor's collision extent, grown by Expand on
+// every axis, against the other actor's own collision primitive (cylinder, box,
+// mesh or BSP brush). No more brush / Level special cases.
 //
-UBOOL AActor::IsOverlapping( const AActor* Other ) const
+UBOOL AActor::IsOverlapping( AActor* Other, FLOAT Expand )
 {
 	guardSlow(AActor::IsOverlapping);
 	checkSlow(Other!=NULL);
 
-	if( !IsBrush() && !Other->IsBrush() && Other!=Level )
-	{
-		// See if cylinder actors are overlapping.
-		return
-			Square(Location.X      - Other->Location.X)
-		+	Square(Location.Y      - Other->Location.Y)
-		<	Square(CollisionRadius + Other->CollisionRadius) 
-		&&	Square(Location.Z      - Other->Location.Z)
-		<	Square(CollisionHeight + Other->CollisionHeight);
-	}
-	else
-	{
-		// We cannot detect whether these actors are overlapping so we say they aren't.
-		return 0;
-	}
+	FCheckResult Hit(1.f);
+	FVector Extent = GetPrimitive()->GetCollisionExtent(this) + FVector(Expand,Expand,Expand);
+	return Other->GetPrimitive()->PointCheck( Hit, Other, Location, Extent, 0 )==0;
+
 	unguardSlow;
 }
```

The order of evaluation matches retail: this primitive's extent first, then `Other->GetPrimitive()`.

---

## 2. ULevel::MoveActor — proto `103b22f0` → retail `103b3e80`

**Source:** `Source/Engine/Src/UnLevAct.cpp:892`. Confirmed by `check(Actor!=NULL)`: proto assert line 900; our source
is 5 lines further down at 905. The touch and untouch blocks are at lines 1064-1076.

Normalised diff, real changes only:

1. **New touch filter** (retail 103b4c2c..103b4c3d, inside the `for(Test=FirstHit; Test && Test->Time<Hit.Time; ...)`
   loop, evaluated first):
   ```
   fld [ebp+0x8]      ; TestAdjust (compiler reused the Actor arg slot; 2.f, or 0.f for a moving brush)
   fadd [ebp-0x28]    ; DeltaSize (0 if Delta nearly zero, else Delta.Size())
   fmul [esi+0x24]    ; Test->Time
   fcomp [ebp-0x28]   ; < DeltaSize ?  (C0 tested; je skip)
   ```
   Only hits that lie within the real move are touched. Hits found only in the extra 2-unit `TestAdjust` look-ahead
   no longer touch.
2. **Untouch:** `push 0x3dcccccd` (0.1f), then `IsOverlapping(Touching[i], 0.1f)`.

Everything else is layout-only: register and stack allocation, and the FVector ctor inlined in one build only. The
Ghidra `param_10 = 0` in the based-actors loop is the compiler storing an IsA result into a dead argument slot
(103b496c..103b4976); it is not a change. Line-number evidence: CheckEncroachment's `check(Actor)` moved from 1207 to
1209, so retail added 2 source lines between MoveActor's check (900) and CheckEncroachment. The touch filter
accounts for at least one of them.

**Patch (confidence: high):**

```diff
--- a/Source/Engine/Src/UnLevAct.cpp
+++ b/Source/Engine/Src/UnLevAct.cpp
@@ -1064,15 +1064,17 @@
 		// Handle Touch notifications.
 		if( MaybeTouched || !Actor->bBlockActors || !Actor->bBlockPlayers )
 			for( FCheckResult* Test=FirstHit; Test && Test->Time<Hit.Time; Test=Test->GetNext() )
 				if
-				(	(!Test->Actor->IsBasedOn(Actor))
+				(	((TestAdjust + DeltaSize) * Test->Time < DeltaSize)	// retail: ignore hits only in the TestAdjust look-ahead
+				&&	(!Test->Actor->IsBasedOn(Actor))
 				&&	(!bIgnoreBases || !Actor->IsBasedOn(Test->Actor))
 				&&	(!Actor->IsBlockedBy(Test->Actor)) )
 					Actor->BeginTouch( Test->Actor );
 
 		// UnTouch notifications.
 		for( int i=0; i<ARRAY_COUNT(Actor->Touching); i++ )
-			if( Actor->Touching[i] && !Actor->IsOverlapping(Actor->Touching[i]) )
+			if( Actor->Touching[i] && !Actor->IsOverlapping(Actor->Touching[i], 0.1f) )
 				Actor->EndTouch( Actor->Touching[i], 0 );
 	}
```

---

## 3. ULevel::CheckEncroachment — proto `103b3a30` → retail `103b55c0`

**Source:** `Source/Engine/Src/UnLevAct.cpp:1203` (`check(Actor)`: proto line 1207, retail 1209, ours 1212).
The normalised diff shows only the assert line number, the SEH address, and `push 0x3f000000` (0.5f) before the
`IsOverlapping` call at 103b5bcc. Retail call site: `IsOverlapping(Touching[i], 0.5f)`.

**Patch (confidence: high):**

```diff
--- a/Source/Engine/Src/UnLevAct.cpp
+++ b/Source/Engine/Src/UnLevAct.cpp
@@ -1270,7 +1270,7 @@
 	{
 		// UnTouch notifications.
 		for( int i=0; i<ARRAY_COUNT(Actor->Touching); i++ )
-			if( Actor->Touching[i] && !Actor->IsOverlapping(Actor->Touching[i]) )
+			if( Actor->Touching[i] && !Actor->IsOverlapping(Actor->Touching[i], 0.5f) )
 				Actor->EndTouch( Actor->Touching[i], 0 );
 	}
```

These are the only callers of IsOverlapping in retail Engine.dll (`callers-retail.tsv` and a full-disassembly grep
for both the stub and the body).

---

## 4. APawn::physWalking — proto `103f2200` → retail `103f4130`

**Source:** `Source/Engine/Src/UnPhysic.cpp:714`. Identified by the FellOutOfWorld/Logf prologue, `calcVelocity`,
the ledge logic, `stepUp`, the `Falling`/`MayFall`/`DoJump` events, auto-jump and the drop-to-floor block.

The normalised diff has 83 hunks; one is a real change. The rest are register allocation, plus the FVector ctor
inlined in proto but called out-of-line in retail (or the reverse) at the sloped-wall CheckLoc, auto-jump, slide and
velocity computations. All of these were checked to compute the same expressions.

**Real change (retail 103f6cc9..103f6d25), drop-to-floor block:**

```
MoveActor(this, Down, Rotation, Hit); if (Hit.Actor!=Base) SetBase(Hit.Actor);
if (IsA(APawn) && Physics==3 /*Swimming*/) -> startSwimming(...); return     (unchanged, now shares a tail)
if (IsA(APawn) && Physics==2 /*Falling*/)  -> jmp 103f768c = plain function epilogue (return)   <-- NEW
else fall through to "if( !bMustJump && Hit.Time<1.f && Hit.Normal.Z>=0.7f )"
```

(In proto, `Physics==3` called startSwimming in place and every other case went straight to the `!bMustJump` block.)
This means that when the step-down move or `SetBase` (via BaseChange/Landed/script) put the pawn into PHYS_Falling,
retail stops processing walking for this tick. It does not go on to slide or `FarMoveActor` back.

**Patch (confidence: high):**

```diff
--- a/Source/Engine/Src/UnPhysic.cpp
+++ b/Source/Engine/Src/UnPhysic.cpp
@@ -1102,11 +1102,14 @@
 				GetLevel()->MoveActor(this, Down, Rotation, Hit);
 				if (Hit.Actor != Base)
 					SetBase(Hit.Actor);
 				if ( IsA(APawn::StaticClass()) && (Physics == PHYS_Swimming) ) //just entered water
 				{
 					((APawn *)this)->startSwimming(Velocity, timeTick, remainingTime, Iterations);
 					return;
 				}
+				// Retail 021009: the step-down / SetBase may have started a fall - stop walking this tick.
+				if ( IsA(APawn::StaticClass()) && (Physics == PHYS_Falling) )
+					return;
 			}
 			else if ( FloorDist < 1.9 )
 			{
```

---

## 5. AInterpolationManager::performPhysics — proto `10401f30` → retail `10403e60`

**Source:** `Source/Engine/Src/UnPhysic.cpp:2666`.

Layout-only: every manager field after `Dest` moves by +4 (`PhysAlpha` 0x2b0→0x2b4, `PhysRate` →0x2b8,
`RemainingPause` →0x2bc, `StartPause` →0x2c0, `PauseNum` →0x2c4, `TurnRateX/Z`, `OldDesiredX/Z` +4). The ends of the
function also differ in FVector inlining.

**Real change A: `Dest->Prev` becomes `Last` (manager +0x2b0 = 688).** Proto reads `Dest->Prev` (`[Dest+0x400]`) in
three places. Retail reads `[this+0x2b0]` in all three:

- 104044d4..10404554: `if( Last ) { OldCoords = GMath.UnitCoords / Last->Rotation; OldCoords.Origin = Last->Location; StartControlPoint = Last->StartControlPoint; }`
  (Last+0x128 Rotation, +0x11c Location, +0x408 StartControlPoint). The `else` branch is unchanged
  (`Dest->Rotation`, `Owner->Location`, zero control point).
- 104049d9..10404a36: `if( Last ) Smoothing = (Last->Smoothing==0.f) ? 0.f : 1.f/Last->Smoothing;` (Last+0x42c).

Retail still reads `Dest->Prev` only in the debug log below. No C++ code writes `Last`; script sets it
(InterpolationManager.Init and InterpolationPoint, per the brief).

**Real change B: debug log (dead in practice).** Retail 104041fc..104042ea, placed right after the `if(!Dest) bDone=1;
else {` and before `if( RemainingPause > 0.f )`:
`if( Owner->CollisionRadius == 49.4 /*double*/ ) GLog->Logf( NAME_Log /*0x87*/, TEXT("IPSnitch To=%s:%i, From=%s:%i, Prev=%s:%i, PhysAlpha=%f, Location=( %f,%f,%f )"), ... )`.
The arguments, decoded from the push order, are: `*Dest->Tag`, `Dest->Position` (+0x2ac),
`Last ? *Last->Tag : TEXT("<null>")`, `Last ? Last->Position : 0`, the same pair for `Dest->Prev`, then `PhysAlpha`
(the just-clamped value) and `Owner->Location.X/Y/Z`. The compare is `fcomp QWORD` against the double 49.4. A float
`CollisionRadius` can never equal that double (49.4f widens to 49.400001525...), so the log can never fire. Porting
it is optional and costs nothing.

**Patch (confidence: high for A; B is optional):**

```diff
--- a/Source/Engine/Src/UnPhysic.cpp
+++ b/Source/Engine/Src/UnPhysic.cpp
@@ -2735,6 +2735,14 @@
 		if ( !Dest )
 			bDone = 1;
 		else 
 		{
+			// Retail 021009 debug trace (dead: float CollisionRadius never == double 49.4).
+			if ( Owner->CollisionRadius == 49.4 )
+				debugf( NAME_Log, TEXT("IPSnitch To=%s:%i, From=%s:%i, Prev=%s:%i, PhysAlpha=%f, Location=( %f,%f,%f )"),
+					*Dest->Tag, Dest->Position,
+					Last ? *Last->Tag : TEXT("<null>"), Last ? Last->Position : 0,
+					Dest->Prev ? *Dest->Prev->Tag : TEXT("<null>"), Dest->Prev ? Dest->Prev->Position : 0,
+					PhysAlpha, Owner->Location.X, Owner->Location.Y, Owner->Location.Z );
 			if( RemainingPause > 0.f )
 			{
 				DesiredSpeed = 0.f;
@@ -2771,15 +2779,15 @@
 				eventUpdateCamera(OldAlpha);
 				FCoords OldCoords;
 				FVector StartControlPoint;
-				if( Dest->Prev )
+				if( Last )	// retail: the point travelled FROM (set by script), not Dest->Prev
 				{
-					//if( Dest->Prev->bDontAlterRotation )
+					//if( Last->bDontAlterRotation )
 					//	OldCoords = GMath.UnitCoords / Owner->Rotation;
 					//else
-						OldCoords = GMath.UnitCoords / Dest->Prev->Rotation;
+						OldCoords = GMath.UnitCoords / Last->Rotation;
 
-					OldCoords.Origin = Dest->Prev->Location;
-					StartControlPoint = Dest->Prev->StartControlPoint;
+					OldCoords.Origin = Last->Location;
+					StartControlPoint = Last->StartControlPoint;
 				}
 				else
 				{
@@ -2830,12 +2838,12 @@
 				FLOAT Smoothing = 1.f;
-				if ( Dest->Prev )
+				if ( Last )
 				{
-					if ( Dest->Prev->Smoothing == 0.f )
+					if ( Last->Smoothing == 0.f )
 						Smoothing = 0.f;
 					else
-						Smoothing = 1.f/Dest->Prev->Smoothing;
+						Smoothing = 1.f/Last->Smoothing;
 				}
```

(`Last` is already declared as `AInterpolationPoint*` at offset 688 in EngineClasses.h. `Position` and `Tag` are
existing members. `debugf` is `GLog->Logf`, and the retail call is the `Logf(EName, const TCHAR*, ...)` overload with
EName 0x87, which is the NAME_Log index used throughout the DLL.)

---

## 6. AActor::CheckHearSound — proto `10416940` → retail `104189d0`

**Source:** `Source/Engine/Src/UnScript.cpp:1126`. Identified by the APlayerPawn ViewTarget (+0x620) hear source,
`RadiusSquared*0.769231` (`/1.3f`), `UModel::FastLineCheck`, `Instigator` (+0xa4) and `*0.6f`/`*0.35f`, then
`eventClientHearSound`. Our source matches proto.

**Real change (retail 10418a84..10418a8d):** before the FastLineCheck, `eax=Id; and eax,0xe; cmp al,0xa; je
<skip occlusion>`. `(Id & 0xE) == 10` means `Slot == 5 == SLOT_Talk` (Id = Index*16 + Slot*2 + bNoOverride). Talk
(dialogue) sounds always play at full radius and volume, with no line-of-sight reduction. The rest of the function is
identical apart from register choice.

**Patch (confidence: high):**

```diff
--- a/Source/Engine/Src/UnScript.cpp
+++ b/Source/Engine/Src/UnScript.cpp
@@ -1137,7 +1137,8 @@
 	FLOAT DistSq = (HearSource-Location).SizeSquared();
 	if( DistSq < NewRadiusSquared )
 	{
-		if ( !GetLevel()->Model->FastLineCheck(HearSource,Location) )
+		// Retail 021009: SLOT_Talk sounds are never occluded ((Id & 0xE) == SLOT_Talk*2).
+		if ( (Id & 0x0E) != (SLOT_Talk << 1) && !GetLevel()->Model->FastLineCheck(HearSource,Location) )
 		{
 			// if no line of sight, reduce radius and volume
 			if ( Instigator != Hearer )
```

---

## 7. FMovingBrushTracker::Update (proto `103935a0` → retail `103949a0`) and the new FlushAllRelatedMovers (retail-only `10394660`)

**Source:** `Source/Engine/Src/UnDynBsp.cpp:254` (`FMovingBrushTracker::Update`; `ABrush::BuildCoords`, `FPoly::Transform`,
`FSphere`, `PrecomputeSphereFilter`). Assert `check(NodeMaps(...).Actor==Actor)` is at proto line 343 (ours 345) and
retail line 411. Retail therefore added 68 lines to UnDynBsp.cpp before that point.

**Update, real change (retail 10394c2a):** after the inlined `AddGroupActor( Actor )` in the
`else if( SavedPos!=Location || SavedRot!=Rotation )` branch, there is `push esi(Actor); mov ecx,ebx(this); call
10394660`. The jump for an already-assimilated actor (`test ah,0x8; jne`) lands on the push, so the call runs whether
or not AddGroupActor did anything. Everything else is register allocation and the assert line.

**10394660** is named `FMovingBrushTracker::FlushAllRelatedMovers` by its own guard string at 0x1047c094. It is
`__thiscall`, takes one argument (`ret 0x4`), returns void and has a guard/unguard SEH frame. Exact logic:

```
if( GroupActors(+0x3c) && Actor && Actor->Brush(+0x188)->MoverLink(+0xf8) != INDEX_NONE )
    Node = &Level(+4)->Model(+0x98)->Nodes(+0x58 data)[Actor->Brush->MoverLink]        // FBspNode, 64 bytes
    for i in 0 .. Level->Actors.Num()-1:
        if Actors(i) && Actors(i)->IsA(AMover) && Other->Brush->MoverLink != INDEX_NONE:
            ON = &Level->Model->Nodes(Other->Brush->MoverLink)
            if (Node->ZoneMask(+0x10) & ((QWORD)1 << ON->iZone[0](+0x34)))
            && (Node->ZoneMask & ((QWORD)1 << ON->iZone[1](+0x35)))
            && Other->SavedPos(+0x470) == Other->Location
            && Other->SavedRot(+0x494) == Other->Rotation
                AddGroupActor(Other)    // inlined: !bAssimilated(0x800 @+0x28) -> set, link into GroupActors, SavedPos/SavedRot = Location/Rotation
```

In retail address order the function comes right after `Flush` (103944f0) and before `Update`, so define it there.
It also needs a declaration in the class's private section.

**Patch (confidence: high on logic; the name comes from the guard string; placement is inferred from address order
and the +68 line shift):**

```diff
--- a/Source/Engine/Src/UnDynBsp.cpp
+++ b/Source/Engine/Src/UnDynBsp.cpp
@@ -96,6 +96,7 @@
 	void SetupActorBrush( AMover* Actor );
 	void FlushActorBrush( AMover* Actor );
+	void FlushAllRelatedMovers( AMover* Actor );	// retail 021009
 	void ForceGroupFlush( INT iNode );
 	void FilterFPoly( INT iAddSurf, INT iNode, INT iCoplanarParent, FPoly *EdPoly, INT Outside );
 	void AddPolyFragment( INT iAddSurf, INT iNode, INT iCoplanarParent, INT IsFront, FPoly* EdPoly );
@@ -249,6 +250,39 @@
 	unguard;
 }
 
+//
+// Retail 021009: when a mover moves, also re-add every stationary mover whose
+// first Bsp node lies in zones covered by the moving mover's first node, so
+// that their Bsp fragments are rebuilt together.
+//
+void FMovingBrushTracker::FlushAllRelatedMovers( AMover* Actor )
+{
+	guard(FMovingBrushTracker::FlushAllRelatedMovers);
+	if( GroupActors && Actor && Actor->Brush->MoverLink!=INDEX_NONE )
+	{
+		FBspNode* Node = &Level->Model->Nodes(Actor->Brush->MoverLink);
+		for( INT i=0; i<Level->Actors.Num(); i++ )
+		{
+			AMover* Other = Cast<AMover>( Level->Actors(i) );
+			if( Other && Other->Brush->MoverLink!=INDEX_NONE )
+			{
+				FBspNode* OtherNode = &Level->Model->Nodes(Other->Brush->MoverLink);
+				if
+				(	(Node->ZoneMask & ((QWORD)1 << OtherNode->iZone[0]))
+				&&	(Node->ZoneMask & ((QWORD)1 << OtherNode->iZone[1]))
+				&&	Other->SavedPos==Other->Location
+				&&	Other->SavedRot==Other->Rotation )
+					AddGroupActor( Other );
+			}
+		}
+	}
+	unguard;
+}
+
 //
 // Update an actor's brush.
 //
@@ -268,6 +302,7 @@
 	{
 		// Update the single brush.
 		AddGroupActor( Actor );
+		FlushAllRelatedMovers( Actor );
 	}
 
 	// Make a complete list of all actors that need flushing.
```

Notes: retail has no `Other!=Actor` test. The moving actor is already `bAssimilated`, so the inlined AddGroupActor
skips it. Keep `AddGroupActor` (not a hand-inlined copy) so the `bAssimilated` gate matches retail. `Nodes(...)` in
release has no bounds check, which matches retail's direct `Data + idx*64`.

---

## Layout-only / no-change items (for the record)

- All pairs: the SEH handler address pushed in the prologue differs, and the `__CxxThrowException`/catch thunk
  addresses differ.
- MoveActor/CheckEncroachment: assert line 1207→1209 (2 lines added in UnLevAct.cpp between 900 and 1207). Our
  source is +5 lines relative to proto throughout this file.
- physWalking: about 80 hunks of register, stack and FVector-inlining noise. All were checked to be the same
  arithmetic, including Slide, CheckLoc+100*Dir, auto-jump EdgeLoc/Predict arguments, the Size2D remaining-time
  calculation, and the falling-velocity recompute. In one retail branch the compiler reuses the `deltaTime` argument
  slot as scratch; that is not a change because the branch returns.
- performPhysics: +4 shift of every field after `Dest`; FVector inlining at the end-of-function velocity and
  acceleration code.
- Update: assert line 343→411; register allocation.
- CheckHearSound: register choice (ebx→ebp) only, apart from the SLOT_Talk test.
