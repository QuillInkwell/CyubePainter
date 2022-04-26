#include "GameAPI.h"
#include <list>
#include "Mod.h"

/************************************************************
	Config Variables (Set these to whatever you need. They are automatically read by the game.)
*************************************************************/
float TickRate = 1;

const int PaintBlock = 3022;
const int UndoBlock = 3023;
const int Marker1Block = 3024;
const int Marker2Block = 3025;
const int MaskBlock = 3026;
const int AirFilter = 3027;

CoordinateInBlocks marker1Cord;
CoordinateInBlocks marker2Cord;
CoordinateInBlocks maskCord;
CoordinateInBlocks paintCord;

bool maskBlockPlaced = false;

UniqueID ThisModUniqueIDs[] = { PaintBlock, UndoBlock, Marker1Block, Marker2Block, MaskBlock };

struct HistoryBlock {
	BlockInfo blockInfo;
	CoordinateInBlocks location;

	HistoryBlock(BlockInfo info, CoordinateInBlocks cords) {
		blockInfo = info;
		location = cords;
	}
};

std::list<HistoryBlock> lastPaintOperation;

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

void PaintArea() {
	bool useMask = false;
	std::list<BlockInfo> maskBlocks;
	BlockInfo targetBlock;

	// Markers must within loaded chunks
	if (GetBlock(marker1Cord).Type == EBlockType::Invalid) {
		SpawnHintText(
			GetPlayerLocation(),
			L"Marker 1 Not Found",
			1,
			1,
			1
		);
		return;
	}
	if (GetBlock(marker2Cord).Type == EBlockType::Invalid) {
		SpawnHintText(
			GetPlayerLocation(),
			L"Marker 2 Not Found",
			1,
			1,
			1
		);
		return;
	}
	// Set the paint target
	if (GetBlock(paintCord).Type == EBlockType::Invalid) {
		SpawnHintText(
			GetPlayerLocation(),
			L"Paint Block Not Found",
			1,
			1,
			1
		);
		return;
	}
	else {
		targetBlock = GetBlock(CoordinateInBlocks(paintCord.X, paintCord.Y, paintCord.Z + 1));
	}
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

void UndoLastOperation() {
	for (HistoryBlock hb : lastPaintOperation) {
		SetBlock(hb.location, hb.blockInfo);
	}
	lastPaintOperation.clear();
}
/************************************************************* 
//	Functions (Run automatically by the game, you can put any code you want into them)
*************************************************************/

// Run every time a block is placed
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

// Run every time a block is destroyed
void Event_BlockDestroyed(CoordinateInBlocks At, UniqueID CustomBlockID, bool Moved)
{
	if (CustomBlockID == MaskBlock) {
		maskBlockPlaced = false;
		maskCord = CoordinateInBlocks(0, 0, 0);
	}
}


// Run every time a block is hit by a tool
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

// Run X times per second, as specified in the TickRate variable at the top
void Event_Tick()
{

}



// Run once when the world is loaded
void Event_OnLoad()
{

}

// Run once when the world is exited
void Event_OnExit()
{
	
}
/*******************************************************

	For all the available game functions you can call, look at the GameAPI.h file

*******************************************************/

