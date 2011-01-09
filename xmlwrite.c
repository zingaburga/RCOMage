
// TODO: htmlspecialchars anywhere?

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "general.h"
#include "rcomain.h"
#include "xml.h"

#define IMP(a,b) (!(a) || (b)) // logical implication, ie, a implies b

void xmlwrite_entry(rRCOEntry* entry, uint depth, rRCOFile* rco, FILE* fp, char* textDir, Bool textXmlOut, int sndDumped, Bool vsmxConv);
void xmlwrite_entry_extra_object(uint16 type, uint8* info, rRCOFile* rco, FILE* fp);
void xmlwrite_entry_extra_anim(uint16 type, uint8* info, rRCOFile* rco, FILE* fp);

void xml_fputref(rRCORef* ref, rRCOFile* rco, FILE* fp);

void rcoxml_fput_escstr(FILE* fp, char* str);

Bool write_xml(rRCOFile* rco, FILE* fp, char* textDir, Bool textXmlOut, int sndDumped, Bool vsmxConv) {
	
	fputs("<?xml version=\"1.0\" encoding=\"iso-8859-1\"?>\n", fp);
	fprintf(fp, "<!-- This XML representation of an RCO structure was generated by %s -->\n", APPNAME_VER);
	fprintf(fp, "<RcoFile UMDFlag=\"%d\" rcomageXmlVer=\"%g\"", rco->umdFlag, APPXMLVER);
	if(rco->ps3) fprintf(fp, " type=\"ps3\"");
	else fprintf(fp, " type=\"psp\"");
	if(rco->verId) {
		fputs(" minFirmwareVer=\"", fp);
		switch(rco->verId) {
			case 0x70: fputs("1.0", fp); break;
			case 0x71: fputs("1.5", fp); break;
			case 0x90: fputs("2.6", fp); break;
			case 0x95: fputs("2.7", fp); break;
			case 0x96: fputs("2.8", fp); break;
			case 0x100: fputs("3.5", fp); break;
			//case 0x107: fputs("ps3", fp); break;
			default: fprintf(fp, "unknownId0x%x", rco->verId);
		}
		fputs("\"", fp);
	}
	fputs(">\n", fp);
	// write entries
	xmlwrite_entry(&(rco->tblMain), 1, rco, fp, textDir, textXmlOut, sndDumped, vsmxConv);
	
	fputs("</RcoFile>\n", fp);
	
	fclose(fp);
	return TRUE;
}


void xmlwrite_entry(rRCOEntry* entry, uint depth, rRCOFile* rco, FILE* fp, char* textDir, Bool textXmlOut, int sndDumped, Bool vsmxConv) {
	uint i;
	char dummy[50] = "\0";
	char* tagName = dummy;
	
	if(entry->id < RCOXML_TABLE_TAGS_NUM) {
		uint maxType = 0;
		while(RCOXML_TABLE_TAGS[entry->id][maxType][0])
			maxType++;
		if(entry->type < maxType) {
			strcpy(tagName, RCOXML_TABLE_TAGS[entry->id][entry->type]);
		} else {
			if(RCOXML_TABLE_NAMES[entry->id][0]) {
				sprintf(tagName, "%sUnknown_0x%x", RCOXML_TABLE_NAMES[entry->id], entry->type);
			} else {
				sprintf(tagName, "Unknown_0x%x_0x%x", entry->id, entry->type);
			}
		}
	} else {
		sprintf(tagName, "Unknown_0x%x_0x%x", entry->id, entry->type);
	}
	
	/*
	switch(entry->id) {
		case RCO_TABLE_MAIN:
			strcpy(tagName, "Main"); break;
		case RCO_TABLE_IMG:
			strcpy(tagName, "Image"); break;
		case RCO_TABLE_SOUND:
			strcpy(tagName, "Sound"); break;
		case RCO_TABLE_MODEL:
			strcpy(tagName, "Model"); break;
		case RCO_TABLE_TEXT:
			strcpy(tagName, "Text"); break;
		case RCO_TABLE_VSMX:
			strcpy(tagName, "VSMX"); break;
		case RCO_TABLE_OBJ:
			if(entry->type <= RCO_OBJ_EXTRA_LEN_NUM && entry->type != 0xB) {
				//char* objTags[] = {"Object", "Page", "Plane", "Button", "XMenu", "XMList", "XList", "Progress", "Scroll", "MList", "MItem", "ObjUnknown0xB", "XItem", "Text", "ModelObject", "Spin", "Action", "ItemSpin", "Group", "LList", "LItem", "Edit", "Clock", "IList", "IItem", "Icon", "UButton"}; // note "ObjUnknown0xB" doesn't exist
				//tagName = objTags[entry->type];
				tagName = (char*)(RCOXML_TABLE_TAGS[RCO_TABLE_OBJ][entry->type]);
			}
			else
				sprintf(tagName, "ObjUnknown0x%x", entry->type);
			break;
		case RCO_TABLE_ANIM:
			if(entry->type <= RCO_ANIM_EXTRA_LEN_NUM) {
				char* animTags[] = {"Anim", "Animation", "Move", "ColourChange", "Rotate", "Resize", "Fade", "Delay", "FireEvent", "Lock", "Unlock", "UnknownB"};
				tagName = animTags[entry->type];
			}
			else
				sprintf(tagName, "AnimUnknown0x%x", entry->type);
			break;
	}
	*/
	
	for(i=0; i<depth; i++)
		fputc('\t', fp);
	fprintf(fp, "<%s", tagName);
	
	Bool isMainTable = (entry->type == 0 || (entry->id == RCO_TABLE_MAIN && entry->type == 1));
	//if(isMainTable)
	//	fputs("Table", fp);
	
	if(entry->labelOffset != RCO_NULL_PTR) {
		fputs(" name=\"", fp);
		rcoxml_fput_escstr(fp, rco->labels + entry->labelOffset);
		fputs("\"", fp);
	}
	if(entry->srcFile[0]) {
		fprintf(fp, " src=\"%s\"", entry->srcFile);
		if((IMP(entry->id == RCO_TABLE_TEXT, !textXmlOut && !textDir) && IMP(entry->id == RCO_TABLE_SOUND, !sndDumped) && IMP(entry->id == RCO_TABLE_VSMX, !vsmxConv)) && (entry->srcAddr || entry->srcCompression || entry->srcLen != filesize(entry->srcFile))) {
			fprintf(fp, " srcRange=\"0x%x-0x%x", entry->srcAddr, entry->srcAddr+entry->srcLen);
			if(entry->srcCompression) {
				if(entry->srcCompression == RCO_DATA_COMPRESSION_RCO) {
					fputs(";rco", fp);
				} else {
					char compr[30];
					rcoxml_int_to_text(entry->srcCompression, RCOXML_TABLE_DATA_COMPRESSION, compr);
					fprintf(fp, ";%s[%u]", compr, entry->srcLenUnpacked);
				}
			}
			fputs("\"", fp);
		}
	}
	// extra attribs
	if(isMainTable) {
		/*
		// pointer ordering
		uint numPtrs = 0;
		void* ptrs;
		switch(entry->id) {
			case RCO_TABLE_TEXT:
				ptrs = rco->ptrText;
				numPtrs = rco->numPtrText;
				break;
			case RCO_TABLE_IMG:
				ptrs = rco->ptrImg;
				numPtrs = rco->numPtrImg;
				break;
			case RCO_TABLE_SOUND:
				ptrs = rco->ptrSound;
				numPtrs = rco->numPtrSound;
				break;
			case RCO_TABLE_MODEL:
				ptrs = rco->ptrModel;
				numPtrs = rco->numPtrModel;
				break;
			case RCO_TABLE_OBJ:
				ptrs = rco->ptrObj;
				numPtrs = rco->numPtrObj;
				break;
			case RCO_TABLE_ANIM:
				ptrs = rco->ptrAnim;
				numPtrs = rco->numPtrAnim;
				break;
		}
		if(numPtrs) {
			uint j;
			fputs(" ptrorder=\"", fp);
			for(i=0; i<numPtrs; i++) {
				if(i)
					fputs(",", fp);
				
				fputc('\n', fp);
				for(j=0; j<depth+2; j++) fputc('\t', fp);
				
				// special case for text
				if(entry->id == RCO_TABLE_TEXT) {
					rRCOTextIdxPtr* tip = &(((rRCOTextIdxPtr*)ptrs)[i]);
					if(tip->textEntry && tip->index) {
						char tmp[30];
						rcoxml_int_to_text(((rRCOTextEntry*)tip->textEntry->extra)->lang, RCOXML_TABLE_TEXT_LANG, tmp);
						fprintf(fp, "%s:%s", tmp, rco->labels + tip->index->labelOffset);
					}
				} else {
					if(((rRCOEntry**)ptrs)[i])
						fputs(((rRCOEntry**)ptrs)[i]->labelOffset + rco->labels, fp);
				}
			}
			fputc('\n', fp);
			for(j=0; j<depth; j++) fputc('\t', fp);
			fputs("\"", fp);
		}
		*/
	} else {
		// extra data
		char tmp[50];
		switch(entry->id) {
			case RCO_TABLE_VSMX:
				// do nothing
				break;
			case RCO_TABLE_TEXT:
				{
					rRCOTextEntry* te = (rRCOTextEntry*)entry->extra;
					rcoxml_int_to_text(te->lang, RCOXML_TABLE_TEXT_LANG, tmp);
					fprintf(fp, " language=\"%s\"", tmp);
					rcoxml_int_to_text(te->format, RCOXML_TABLE_TEXT_FMT, tmp);
					fprintf(fp, " format=\"%s\"", tmp);
					if(te->numIndexes && !textXmlOut) { // write text labels
						uint j;
						fputs(" entries=\"", fp);
						for(i=0; i<te->numIndexes; i++) {
							if(i) fputc(',', fp);
							fputc('\n', fp);
							for(j=0; j<depth+2; j++) fputc('\t', fp);
							if(te->indexes[i].labelOffset != RCO_NULL_PTR)
								fputs(rco->labels + te->indexes[i].labelOffset, fp);
						}
						fputc('\n', fp);
						for(j=0; j<depth; j++) fputc('\t', fp);
						fputs("\"", fp);
						
						// write srcParts
						if(!textDir) {
							fputs(" srcParts=\"", fp);
							for(i=0; i<te->numIndexes; i++) {
								if(i) fputc(',', fp);
								fprintf(fp, "0x%x@0x%x", te->indexes[i].length, te->indexes[i].offset);
							}
							fputs("\"", fp);
						}
					}
				}
				break;
			case RCO_TABLE_IMG: case RCO_TABLE_MODEL:
				rcoxml_int_to_text(((rRCOImgModelEntry*)entry->extra)->format, (entry->id == RCO_TABLE_IMG ? RCOXML_TABLE_IMG_FMT : RCOXML_TABLE_MODEL_FMT), tmp);
				fprintf(fp, " format=\"%s\"", tmp);
				rcoxml_int_to_text(((rRCOImgModelEntry*)entry->extra)->compression, RCOXML_TABLE_DATA_COMPRESSION, tmp);
				fprintf(fp, " compression=\"%s\"", tmp);
				fprintf(fp, " unknownByte=\"%d\"", ((rRCOImgModelEntry*)entry->extra)->unkCompr);
				break;
			case RCO_TABLE_SOUND:
				{
					rRCOSoundEntry* se = ((rRCOSoundEntry*)entry->extra);
					rcoxml_int_to_text(se->format, RCOXML_TABLE_SOUND_FMT, tmp);
					fprintf(fp, " format=\"%s\"", tmp);
					if(se->format == RCO_SOUND_VAG && sndDumped != 2)
						fprintf(fp, " channels=\"%d\"", se->channels);
					
					if(se->channels && !sndDumped) {
						// write srcParts
						fputs(" srcParts=\"", fp);
						for(i=0; i<se->channels; i++) {
							if(i) fputc(',', fp);
							fprintf(fp, "0x%x@0x%x", se->channelData[i*2], se->channelData[i*2 +1]);
						}
						fputs("\"", fp);
					}
					
				}
				break;
			case RCO_TABLE_FONT: {
				rRCOFontEntry* rfe = (rRCOFontEntry*)entry->extra;
				
				fprintf(fp, " unknownShort1=\"0x%x\" unknownShort2=\"0x%x\" unknownInt3=\"0x%x\" unknownInt4=\"0x%x\"", rfe->format, rfe->compression, rfe->unknown, rfe->unknown2);
				
			} break;
			case RCO_TABLE_OBJ:
				xmlwrite_entry_extra_object(entry->type, (uint8*)entry->extra, rco, fp);
				break;
			case RCO_TABLE_ANIM:
				xmlwrite_entry_extra_anim(entry->type, (uint8*)entry->extra, rco, fp);
				break;
		}
	}
	
	if(entry->numSubentries) {
		rRCOEntry* rcoNode;
		fputs(">\n", fp);
		
		for(rcoNode=entry->firstChild; rcoNode; rcoNode=rcoNode->next)
			xmlwrite_entry(rcoNode, depth+1, rco, fp, textDir, textXmlOut, sndDumped, vsmxConv);
		
		
		for(i=0; i<depth; i++)
			fputc('\t', fp);
		//fprintf(fp, "</%s%s>\n", tagName, (isMainTable ? "Table" : ""));
		fprintf(fp, "</%s>\n", tagName);
		
	} else {
		if(entry->id == RCO_TABLE_OBJ || entry->type == 0 || (entry->type == 1 && (entry->id == RCO_TABLE_ANIM || entry->id == RCO_TABLE_MAIN || entry->id == RCO_TABLE_VSMX)))
			fprintf(fp, "></%s>\n", tagName);
		else
			fputs(" />\n", fp);
	}
}



void xmlwrite_entry_extra_object(uint16 type, uint8* info, rRCOFile* rco, FILE* fp) {
	
	int numEntries = RCO_OBJ_EXTRA_LEN[type];
	int i=0, i2=0;
	
	if(numEntries < 1) return; // TODO: handle unknown object types?
	
	for(i=0, i2=0; i<numEntries; i++, i2++) { // i2 doesn't include the position entry, plus doesn't increase twice for a reference
		// is this entry a reference?
		Bool isRef = RCO_OBJ_IS_REF(type, i2);
		//if(i2) fputc(' ', fp); // space if not first param
		fputc(' ', fp);
		// do we have a label for this thing?
		if(RCO_OBJ_EXTRA_NAMES[type][i2][0]) {
			fputs(RCO_OBJ_EXTRA_NAMES[type][i2], fp);
		} else {
			fputs("unknown", fp);
			switch(RCO_OBJ_EXTRA_TYPES[type][i2]) {
				case RCO_OBJ_EXTRA_TYPE_FLOAT:
					fputs("Float", fp); break;
				case RCO_OBJ_EXTRA_TYPE_INT:
					fputs("Int", fp); break;
				case RCO_OBJ_EXTRA_TYPE_EVENT:
					fputs("Event", fp); break;
				case RCO_OBJ_EXTRA_TYPE_IMG:
					fputs("Image", fp); break;
				case RCO_OBJ_EXTRA_TYPE_MODEL:
					fputs("Model", fp); break;
				case RCO_OBJ_EXTRA_TYPE_FONT:
					fputs("Font", fp); break;
				case RCO_OBJ_EXTRA_TYPE_OBJ:
					fputs("Object", fp); break;
				case RCO_OBJ_EXTRA_TYPE_UNK: case RCO_OBJ_EXTRA_TYPE_REF:
					if(isRef) fputs("Ref", fp);
					break;
			}
			fprintf(fp, "%d", i);
		}
		fputs("=\"", fp);
		
		if(isRef) {
			rRCORef* ref = (rRCORef*)info;
			
			xml_fputref(ref, rco, fp);
			
			info += sizeof(rRCORef);
		} else {
			if(RCO_OBJ_EXTRA_TYPES[type][i2] == RCO_OBJ_EXTRA_TYPE_FLOAT)
				fprintf(fp, "%g", *(float*)info);
			else
				fprintf(fp, "0x%x", *(uint32*)info);
			
			info += sizeof(uint32); // or sizeof(float)
		}
		
		fputs("\"", fp);
		if(isRef) i++; // need to do this since the tables consider refs to take two entries
	}
}




void xmlwrite_entry_extra_anim(uint16 type, uint8* info, rRCOFile* rco, FILE* fp) {
	
	int numEntries = RCO_ANIM_EXTRA_LEN[type];
	int i=0, i2=0;
	
	if(numEntries < 1) return; // TODO: handle unknown object types?
	
	for(i=0, i2=0; i<numEntries; i++, i2++) { // i2 doesn't include the position entry, plus doesn't increase twice for a reference
		// is this entry a reference?
		Bool isRef = RCO_ANIM_IS_REF(type, i2);
		//if(i2) fputc(' ', fp); // space if not first param
		fputc(' ', fp);
		// do we have a label for this thing?
		if(RCO_ANIM_EXTRA_NAMES[type][i2][0]) {
			fputs(RCO_ANIM_EXTRA_NAMES[type][i2], fp);
		} else {
			fputs("unknown", fp);
			switch(RCO_ANIM_EXTRA_TYPES[type][i2]) {
				case RCO_OBJ_EXTRA_TYPE_FLOAT:
					fputs("Float", fp); break;
				case RCO_OBJ_EXTRA_TYPE_INT:
					fputs("Int", fp); break;
				case RCO_OBJ_EXTRA_TYPE_EVENT:
					fputs("Event", fp); break;
				case RCO_OBJ_EXTRA_TYPE_IMG:
					fputs("Image", fp); break;
				case RCO_OBJ_EXTRA_TYPE_MODEL:
					fputs("Model", fp); break;
				case RCO_OBJ_EXTRA_TYPE_FONT:
					fputs("Font", fp); break;
				case RCO_OBJ_EXTRA_TYPE_OBJ:
					fputs("Object", fp); break;
				case RCO_OBJ_EXTRA_TYPE_UNK: case RCO_OBJ_EXTRA_TYPE_REF:
					if(isRef) fputs("Ref", fp);
					break;
			}
			fprintf(fp, "%d", i);
		}
		fputs("=\"", fp);
		
		if(isRef) {
			rRCORef* ref = (rRCORef*)info;
			
			xml_fputref(ref, rco, fp);
			
			info += sizeof(rRCORef);
		} else {
			if(RCO_ANIM_EXTRA_TYPES[type][i2] == RCO_OBJ_EXTRA_TYPE_FLOAT)
				fprintf(fp, "%g", *(float*)info);
			else
				fprintf(fp, "0x%x", *(uint32*)info);
			
			info += sizeof(uint32); // or sizeof(float)
		}
		
		fputs("\"", fp);
		if(isRef) i++; // need to do this since the tables consider refs to take two entries
	}
}


/*
void xmlwrite_entry_extra_anim(uint16 type, uint8* info, rRCOFile* rco, FILE* fp) {
	int numEntries = RCO_ANIM_EXTRA_LEN[type];
	int i;
	
	if(numEntries < 1) return; // TODO: handle unknown anim types?
	
	// TODO: this method makes the number following unknown tag names disregard the reference (should fix this)
	if(RCO_ANIM_EXTRA_REFS[type]) {
		if(type == RCO_ANIM_TYPE_EVENT)
			fputs(" event=\"", fp);
		else
			fputs(" object=\"", fp);
		xml_fputref((rRCORef*)info, rco, fp);
		fputs("\"", fp);
		
		info += sizeof(rRCORef);
		numEntries -= 2;
	}
	
	for(i=0; i<numEntries; i++) {
		fputc(' ', fp);
		// do we have a label for this thing?
		if(RCO_ANIM_EXTRA_NAMES[type][i][0]) {
			fputs(RCO_ANIM_EXTRA_NAMES[type][i], fp);
		} else {
			fputs("unknown", fp);
			switch(RCO_ANIM_EXTRA_TYPES[type][i]) {
				case RCO_OBJ_EXTRA_TYPE_FLOAT:
					fputs("Float", fp); break;
				case RCO_OBJ_EXTRA_TYPE_INT:
					fputs("Int", fp); break;
			}
			fprintf(fp, "%d", i);
		}
		fputs("=\"", fp);
		
		if(RCO_ANIM_EXTRA_TYPES[type][i] == RCO_OBJ_EXTRA_TYPE_FLOAT)
			fprintf(fp, "%g", *(float*)info);
		else
			fprintf(fp, "0x%x", *(uint32*)info);
		
		info += sizeof(uint32); // or sizeof(float)
		
		fputs("\"", fp);
	}
}
*/


void xml_fputref(rRCORef* ref, rRCOFile* rco, FILE* fp) {
	Bool unkType = FALSE;
	switch(ref->type) {
		case RCO_REF_EVENT: fputs("event:", fp); break;
		case RCO_REF_TEXT: fputs("text:", fp); break;
		case RCO_REF_IMG: fputs("image:", fp); break;
		case RCO_REF_MODEL: fputs("model:", fp); break;
		case RCO_REF_FONT: fputs("font:", fp); break;
		case RCO_REF_OBJ2: fputs("object2:", fp); break;
		case RCO_REF_ANIM: fputs("anim:", fp); break;
		case RCO_REF_OBJ: fputs("object:", fp); break;
		case RCO_REF_NONE: fputs("nothing", fp); break;
		default:
			fprintf(fp, "unknown0x%x:", ref->type);
			unkType = TRUE;
	}
	if(unkType) {
		fprintf(fp, "0x%x", ref->rawPtr);
	} else if(ref->type == RCO_REF_EVENT) {
		//fputs((char*)ref->ptr, fp);
		fputs(rco->events + ref->rawPtr, fp);
	} else if(ref->type == RCO_REF_TEXT) {
		// TODO: check this
		// assume first text lang is accurate
		if(rco->tblText && rco->tblText->numSubentries)
			fputs(((rRCOTextEntry*)(rco->tblText->firstChild->extra))->indexes[ref->rawPtr].labelOffset + rco->labels, fp);
	} else if(ref->type != RCO_REF_NONE) {
		if(((rRCOEntry*)(ref->ptr))->labelOffset != RCO_NULL_PTR)
			fputs(((rRCOEntry*)(ref->ptr))->labelOffset + rco->labels, fp);
		else
			fputs("", fp); // TODO: handle situations where there isn't a label...
	}
}

void rcoxml_int_to_text(uint in, const RcoTableMap map, char* out) {
	uint len=0;
	// determine length of map
	while(map[len][0]) len++;
	
	// perhaps think about allowing blank/unknown values in midle of map?
	if(in < len) {
		strcpy(out, map[in]);
		return;
	}
	sprintf(out, "unknown0x%x", in);
}


// custom reserved characters: ',' ':' (kinda)
void rcoxml_fput_escstr(FILE* fp, char* str) {
	while(*str) {
		switch(*str) {
			case '<': fputs("&lt;", fp); break;
			case '>': fputs("&gt;", fp); break;
			case '"': fputs("&quot;", fp); break;
			case '&': fputs("&amp;", fp); break;
			case '\n': 
				//if(allowNL) {
					fputc(*str, fp);
					break;
				//}
			default:
				if(*str < 32)
					fprintf(fp, "&#%d;", *str);
				else
					fputc(*str, fp);
		}
		
		str++;
	}
}