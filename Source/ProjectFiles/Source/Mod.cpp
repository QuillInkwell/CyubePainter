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
const int ToggleWandBlock = 3028;
const int CopyBlock = 3029;
const int CutBlock = 3030;
const int PasteBlock = 3031;
const int Rotate90CWBlock = 3032;
const int RedoBlock = 3033;
const int Rotate90CCWBlock = 3034;

UniqueID ThisModUniqueIDs[] = { 
	PaintBlock, UndoBlock, Marker1Block, Marker2Block, MaskBlock, ToggleWandBlock, CopyBlock, CutBlock, PasteBlock, Rotate90CWBlock, RedoBlock, Rotate90CCWBlock };

// Data Structs
//********************************
struct Block {
	BlockInfo blockInfo;
	CoordinateInBlocks location;

	Block(BlockInfo info, CoordinateInBlocks cords) {
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
bool selectionWandEnabled = false;

std::list<Block> lastPaintOperation;
std::list<Block> lastUndoOperation;
std::list<Block> clipboard;

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
bool Paint(BlockInfo originalBlock, BlockInfo newBlock, CoordinateInBlocks At) {
	lastPaintOperation.push_front(Block(originalBlock, At));
	return SetBlock(At, newBlock);
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

void PaintArea() {
	bool useMask = false;
	std::list<BlockInfo> maskBlocks;

	if (!MarkersInLoadedChunks()) return;

	BlockInfo targetBlock = SetPaintTarget();
	if (targetBlock.Type == EBlockType::Invalid) return;

	// Set the block mask if one exists
	if (maskBlockPlaced && !(GetBlock(maskCord).Type == EBlockType::Invalid)) {
		maskBlocks = GetMaskBlocks();
		if (!(maskBlocks.empty())) {
			useMask = true;
		}
		else {
			SpawnHintText(
				GetPlayerLocation(),
				L"Masking block found but mask is empty! Please ensure your mask blocks are placed above the masking block. If you wish to mask for air use an Air Filter block.",
				1,
				1
			);
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
				Paint(currentBlock, targetBlock, CoordinateInBlocks(x, y, z));
			}
		}
	}
}

// Undo Methods
//********************************
void UndoLastOperation() {
	if (lastPaintOperation.empty()) return;
	
	lastUndoOperation.clear();
	for (const Block& hb : lastPaintOperation) {
		BlockInfo currentBlock = GetBlock(hb.location);
		lastUndoOperation.push_front(Block(currentBlock, hb.location));
		SetBlock(hb.location, hb.blockInfo);
	}
	lastPaintOperation.clear();
}

void RedoLastOperation() {
	if (lastUndoOperation.empty()) return;

	for (const Block& hb : lastUndoOperation) {
		BlockInfo currentBlock = GetBlock(hb.location);
		Paint(currentBlock, hb.blockInfo, hb.location);
	}
	lastUndoOperation.clear();
}

// Clipboard Method
//********************************
void CopyRegion() {
	CoordinateInBlocks startCorner = GetSmallVector(marker1Cord, marker2Cord);
	CoordinateInBlocks endCorner = GetLargeVector(marker1Cord, marker2Cord);
	clipboard.clear();

	for (int16_t z = startCorner.Z; z <= endCorner.Z; z++) {

		for (int64_t y = startCorner.Y; y <= endCorner.Y; y++) {

			for (int64_t x = startCorner.X; x <= endCorner.X; x++) {
				int localX = x - startCorner.X;
				int localY = y - startCorner.Y;
				int localZ = z - startCorner.Z;
				BlockInfo currentBlock = GetBlock(CoordinateInBlocks(x, y, z));

				clipboard.push_front(Block(currentBlock, CoordinateInBlocks(localX, localY, localZ)));
			}
		}
	}
}

void CutRegion() {
	CoordinateInBlocks startCorner = GetSmallVector(marker1Cord, marker2Cord);
	CoordinateInBlocks endCorner = GetLargeVector(marker1Cord, marker2Cord);
	
	clipboard.clear();
	lastPaintOperation.clear();

	for (int16_t z = startCorner.Z; z <= endCorner.Z; z++) {

		for (int64_t y = startCorner.Y; y <= endCorner.Y; y++) {

			for (int64_t x = startCorner.X; x <= endCorner.X; x++) {
				int localX = x - startCorner.X;
				int localY = y - startCorner.Y;
				int localZ = z - startCorner.Z;
				BlockInfo currentBlock = GetBlock(CoordinateInBlocks(x, y, z));

				clipboard.push_front(Block(currentBlock, CoordinateInBlocks(localX, localY, localZ)));
				Paint(currentBlock, EBlockType::Air, CoordinateInBlocks(x, y, z));
			}
		}
	}
}

void PasteClipboard(CoordinateInBlocks At) {
	if (clipboard.empty()) return;
	lastPaintOperation.clear();

	for (const Block& block : clipboard) {
		BlockInfo currentBlock = GetBlock(At + block.location);
		Paint(currentBlock, block.blockInfo, At + block.location);
	}
}

void RotateClipboard90DegreesClockwise() {
	for (Block block : clipboard) {
		int x = block.location.Y;
		int y = block.location.X * -1;
		block.location = CoordinateInBlocks(x, y, block.location.Z);
	}
}

void RotateClipboard90DegreesCounterClockwise() {
	for (Block block : clipboard) {
		int x = block.location.Y * -1;
		int y = block.location.X;
		block.location = CoordinateInBlocks(x, y, block.location.Z);
	}
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
		else if (CustomBlockID == UndoBlock) {
			UndoLastOperation();
		}
		else if (CustomBlockID == ToggleWandBlock) {
			selectionWandEnabled = !selectionWandEnabled;
		}
		else if (CustomBlockID == RedoBlock) {
			RedoLastOperation();
		}
		else if (CustomBlockID == CopyBlock) {
			CopyRegion();
		}
		else if (CustomBlockID == CutBlock) {
			CutRegion();
		}
		else if (CustomBlockID == PasteBlock) {
			PasteClipboard(At);
		}
		else if (CustomBlockID == Rotate90CWBlock) {
			RotateClipboard90DegreesClockwise();
		}
		else if (CustomBlockID == Rotate90CCWBlock) {
			RotateClipboard90DegreesCounterClockwise();
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
	if (selectionWandEnabled) {
		if (ToolName == L"T_Pickaxe_Stone") {
			SpawnHintText(At, L"Marker 1 set!", 1, 1);
			marker1Cord = At;
		}
		if (ToolName == L"T_Axe_Stone") {
			SpawnHintText(At, L"Marker 2 set!", 1, 1);
			marker2Cord = At;
		}
	}
}