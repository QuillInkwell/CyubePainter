#include "GameAPI.h"
#include <list>
#include "Mod.h"

/************************************************************
	Config Variables (Set these to whatever you need. They are automatically read by the game.)
*************************************************************/
float TickRate = 1;

// Unique Mod IDS
//********************************
const int PaintBlock = 3022;
const int UndoBlock = 3023;
const int Marker1Block = 3024;
const int Marker2Block = 3025;
const int MaskBlock = 3026;
const int AirFilter = 3027;

UniqueID ThisModUniqueIDs[] = { PaintBlock, UndoBlock, Marker1Block, Marker2Block, MaskBlock };

// Data Structs
//********************************
struct HistoryBlock {
	BlockInfo blockInfo;
	CoordinateInBlocks location;

	HistoryBlock(BlockInfo info, CoordinateInBlocks cords) {
		blockInfo = info;
		location = cords;
	}
};

// State Variables
//********************************
CoordinateInBlocks marker1Cord;
CoordinateInBlocks marker2Cord;
CoordinateInBlocks maskCord;
CoordinateInBlocks paintCord;

bool maskBlockPlaced = false;

std::list<HistoryBlock> lastPaintOperation;

// Utility Methods
//********************************
CoordinateInBlocks GetSmallVector(CoordinateInBlocks cord1, CoordinateInBlocks cord2) {
	int64_t x = (cord1.X < cord2.X) ? cord1.X : cord2.X;
	int64_t y = (cord1.Y < cord2.Y) ? cord1.Y : cord2.Y;
	int16_t z = (cord1.Z < cord2.Z) ? cord1.Z : cord2.Z;

	return CoordinateInBlocks(x, y, z);
}
CoordinateInBlocks GetLargeVector(CoordinateInBlocks cord1, CoordinateInBlocks cord2) {
	int64_t x = (cord1.X > cord2.X) ? cord1.X : cord2.X;
	int64_t y = (cord1.Y > cord2.Y) ? cord1.Y : cord2.Y;
	int16_t z = (cord1.Z > cord2.Z) ? cord1.Z : cord2.Z;

	return CoordinateInBlocks(x, y, z);
}

// Masking Methods
//********************************
std::list<BlockInfo> GetMaskBlocks()
{
	std::list<BlockInfo> maskBlocks;
	BlockInfo block = GetBlock(maskCord + CoordinateInBlocks(0, 0, 1));
	int i = 1;
	while (block.Type != EBlockType::Air) {
		maskBlocks.push_front(block);
		i++;
		block = GetBlock(maskCord + CoordinateInBlocks(0, 0, i));
	}
	return maskBlocks;
}
bool BlockIsMaskTarget(BlockInfo info, std::list<BlockInfo> mask) {
	for (BlockInfo maskInfo : mask) {
		if (maskInfo.CustomBlockID == AirFilter && info.Type == EBlockType::Air)
			return true;
		if (info.Type == maskInfo.Type && info.CustomBlockID == maskInfo.CustomBlockID)
			return true;
	}
	return false;
}

// Paint Methods
//********************************
void PaintArea() {
	bool useMask = false;
	std::list<BlockInfo> maskBlocks;

	if (!MarkersInLoadedChunks) return;

	BlockInfo targetBlock = SetPaintTarget();
	if (targetBlock.Type == EBlockType::Invalid) return;

	// Set the block mask if one exists
	if (maskBlockPlaced && !(GetBlock(maskCord).Type == EBlockType::Invalid)) {
		maskBlocks = GetMaskBlocks();
		if (!(maskBlocks.empty())) {
			useMask = true;
		}
	}

	CoordinateInBlocks startCorner = GetSmallVector(marker1Cord, marker2Cord);
	CoordinateInBlocks endCorner = GetLargeVector(marker1Cord, marker2Cord);
	lastPaintOperation.clear();

	for (int16_t z = startCorner.Z; z <= endCorner.Z; z++) {

		for (int64_t y = startCorner.Y; y <= endCorner.Y; y++) {

			for (int64_t x = startCorner.X; x <= endCorner.X; x++) {
				
				BlockInfo currentBlock = GetBlock(CoordinateInBlocks(x, y, z));
				if (useMask && !(BlockIsMaskTarget(currentBlock, maskBlocks)) ) {
					continue;
				}
				lastPaintOperation.push_front(HistoryBlock(
					GetBlock(CoordinateInBlocks(x, y, z)),
					CoordinateInBlocks(x, y, z)
				));
				SetBlock(CoordinateInBlocks(x, y, z), targetBlock);
			}
		}
	}
}

bool MarkersInLoadedChunks() {
	if (GetBlock(marker1Cord).Type == EBlockType::Invalid) {
		SpawnHintText(
			GetPlayerLocation(),
			L"Marker 1 Not Found",
			1,
			1,
			1
		);
		return false;
	}
	if (GetBlock(marker2Cord).Type == EBlockType::Invalid) {
		SpawnHintText(
			GetPlayerLocation(),
			L"Marker 2 Not Found",
			1,
			1,
			1
		);
		return false;
	}
	return true;
}

BlockInfo SetPaintTarget() {
	if (GetBlock(paintCord).Type == EBlockType::Invalid) {
		SpawnHintText(
			GetPlayerLocation(),
			L"Paint Block Not Found",
			1,
			1,
			1
		);
		return BlockInfo(EBlockType::Invalid);
	}
	else {
		return GetBlock(CoordinateInBlocks(paintCord.X, paintCord.Y, paintCord.Z + 1));
	}
}


// Undo Methods
//********************************
void UndoLastOperation() {
	for (HistoryBlock hb : lastPaintOperation) {
		SetBlock(hb.location, hb.blockInfo);
	}
	lastPaintOperation.clear();
}

/************************************************************* 
//	Event Functions
*************************************************************/

void Event_BlockPlaced(CoordinateInBlocks At, UniqueID CustomBlockID, bool Moved)
{
	if (CustomBlockID == Marker1Block) {
		marker1Cord = At;
	}
	else if (CustomBlockID == Marker2Block) {
		marker2Cord = At;
	}
	else if (CustomBlockID == MaskBlock) {
		maskCord = At;
		maskBlockPlaced = true;
	}
	else if (CustomBlockID == PaintBlock) {
		paintCord = At;
	}
}

void Event_BlockDestroyed(CoordinateInBlocks At, UniqueID CustomBlockID, bool Moved)
{
	if (CustomBlockID == MaskBlock) {
		maskBlockPlaced = false;
		maskCord = CoordinateInBlocks(0, 0, 0);
	}
}

void Event_BlockHitByTool(CoordinateInBlocks At, UniqueID CustomBlockID, std::wstring ToolName)
{
	if (ToolName == L"T_Stick") {
		if (CustomBlockID == PaintBlock) {
			PaintArea();
		}
		if (CustomBlockID == UndoBlock) {
			UndoLastOperation();
		}
	}
}

void Event_Tick()
{

}

void Event_OnLoad()
{

}

void Event_OnExit()
{
	
}

/*************************************************************
//	Advanced Event Functions
*************************************************************/

void Event_AnyBlockPlaced(CoordinateInBlocks At, BlockInfo Type, bool Moved)
{

}

void Event_AnyBlockDestroyed(CoordinateInBlocks At, BlockInfo Type, bool Moved)
{

}

void Event_AnyBlockHitByTool(CoordinateInBlocks At, BlockInfo Type, std::wstring ToolName)
{

}

