/* GemRB - Infinity Engine Emulator
 * Copyright (C) 2003 The GemRB Project
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include "DialogHandler.h"

#include "strrefs.h"

#include "DialogMgr.h"
#include "DisplayMessage.h"
#include "Game.h"
#include "GameData.h"
#include "Video.h"
#include "GUI/GameControl.h"

DialogHandler::DialogHandler(void)
{
	dlg = NULL;
	targetID = 0;
	originalTargetID = 0;
	speakerID = 0;
	targetOB = NULL;
}

DialogHandler::~DialogHandler(void)
{
	if (dlg) {
		delete dlg;
	}
}

//Try to start dialogue between two actors (one of them could be inanimate)
int DialogHandler::InitDialog(Scriptable* spk, Scriptable* tgt, const char* dlgref)
{
	if (dlg) {
		delete dlg;
		dlg = NULL;
	}

	PluginHolder<DialogMgr> dm(IE_DLG_CLASS_ID);
	dm->Open( gamedata->GetResource( dlgref, IE_DLG_CLASS_ID ), true );
	dlg = dm->GetDialog();

	if (!dlg) {
		printMessage("GameControl", " ", LIGHT_RED);
		printf( "Cannot start dialog: %s\n", dlgref );
		return -1;
	}

	strnlwrcpy(dlg->ResRef, dlgref, 8); //this isn't handled by GetDialog???

	//target is here because it could be changed when a dialog runs onto
	//and external link, we need to find the new target (whose dialog was
	//linked to)

	Actor *spe = (Actor *) spk;
	speakerID = spe->globalID;
	Actor *oldTarget = GetActorByGlobalID(targetID);
	if (tgt->Type!=ST_ACTOR) {
		targetID=0xffff;
		//most likely this dangling object reference
		//won't cause problems, because trigger points don't
		//get destroyed during a dialog
		targetOB=tgt;
		spk->LastTalkedTo=0;
	} else {
		Actor *tar = (Actor *) tgt;
		speakerID = spe->globalID;
		targetID = tar->globalID;
		if (!originalTargetID) originalTargetID = tar->globalID;
		spe->LastTalkedTo=targetID;
		tar->LastTalkedTo=speakerID;
		tar->SetCircleSize();
	}
	if (oldTarget) oldTarget->SetCircleSize();

	//check if we are already in dialog
	if (core->GetGameControl()->GetDialogueFlags()&DF_IN_DIALOG) {
		return 0;
	}

	int si = dlg->FindFirstState( tgt );
	if (si < 0) {
		return -1;
	}

	//we need GUI for dialogs
	core->GetGameControl()->UnhideGUI();

	//no exploring while in dialogue
	core->GetGameControl()->SetScreenFlags(SF_GUIENABLED|SF_DISABLEMOUSE|SF_LOCKSCROLL, BM_OR);
	core->GetGameControl()->SetDialogueFlags(DF_IN_DIALOG, BM_OR);

	if (tgt->Type==ST_ACTOR) {
		Actor *tar = (Actor *) tgt;
		tar->DialogInterrupt();
	}

	//allow mouse selection from dialog (even though screen is locked)
	Video *video = core->GetVideoDriver();
	Region vp = video->GetViewport();
	video->SetMouseEnabled(true);
	core->timer->SetMoveViewPort( tgt->Pos.x, tgt->Pos.y, 0, true );
	video->MoveViewportTo( tgt->Pos.x-vp.w/2, tgt->Pos.y-vp.h/2 );
	//there are 3 bits, if they are all unset, the dialog freezes scripts
	if (!(dlg->Flags&7) ) {
		core->GetGameControl()->SetDialogueFlags(DF_FREEZE_SCRIPTS, BM_OR);
	}
	//opening control size to maximum, enabling dialog window
	core->GetGame()->SetControlStatus(CS_HIDEGUI, BM_NAND);
	core->GetGame()->SetControlStatus(CS_DIALOG, BM_OR);
	core->SetEventFlag(EF_PORTRAIT);
	return 0;
}

/*try to break will only try to break it, false means unconditional stop*/
void DialogHandler::EndDialog(bool try_to_break)
{
	if (try_to_break && (core->GetGameControl()->GetDialogueFlags()&DF_UNBREAKABLE) ) {
		return;
	}

	Actor *tmp = GetSpeaker();
	if (tmp) {
		tmp->LeaveDialog();
	}
	speakerID = 0;
	if (targetID==0xffff) {
		targetOB->LeaveDialog();
	} else {
		tmp=GetTarget();
		if (tmp) {
			tmp->LeaveDialog();
		}
	}
	targetOB = NULL;
	targetID = 0;
	if (tmp) tmp->SetCircleSize();
	originalTargetID = 0;
	ds = NULL;
	if (dlg) {
		delete dlg;
		dlg = NULL;
	}
	//restoring original size
	core->GetGame()->SetControlStatus(CS_DIALOG, BM_NAND);
	core->GetGameControl()->SetScreenFlags(SF_DISABLEMOUSE|SF_LOCKSCROLL, BM_NAND);
	core->GetGameControl()->SetDialogueFlags(0, BM_SET);
	core->SetEventFlag(EF_PORTRAIT);
}

//translate section values (journal, solved, unsolved, user)
static const int sectionMap[4]={4,1,2,0};

void DialogHandler::DialogChoose(unsigned int choose)
{
	TextArea* ta = core->GetMessageTextArea();
	if (!ta) {
		printMessage("GameControl","Dialog aborted???",LIGHT_RED);
		EndDialog();
		return;
	}

	Actor *speaker = GetSpeaker();
	if (!speaker) {
		printMessage("GameControl","Speaker gone???",LIGHT_RED);
		EndDialog();
		return;
	}
	Actor *tgt;
	Scriptable *target;

	if (targetID!=0xffff) {
		tgt = GetTarget();
		target = tgt;
	} else {
		//risky!!!
		target = targetOB;
		tgt=NULL;
	}
	if (!target) {
		printMessage("GameControl","Target gone???",LIGHT_RED);
		EndDialog();
		return;
	}

	Video *video = core->GetVideoDriver();
	Region vp = video->GetViewport();
	video->SetMouseEnabled(true);
	core->timer->SetMoveViewPort( target->Pos.x, target->Pos.y, 0, true );
	video->MoveViewportTo( target->Pos.x-vp.w/2, target->Pos.y-vp.h/2 );

	if (choose == (unsigned int) -1) {
		//increasing talkcount after top level condition was determined

		int si = dlg->FindFirstState( tgt );
		if (si<0) {
			EndDialog();
			return;
		}

		if (tgt) {
			if (core->GetGameControl()->GetDialogueFlags()&DF_TALKCOUNT) {
				core->GetGameControl()->SetDialogueFlags(DF_TALKCOUNT, BM_NAND);
				tgt->TalkCount++;
			} else if (core->GetGameControl()->GetDialogueFlags()&DF_INTERACT) {
				core->GetGameControl()->SetDialogueFlags(DF_INTERACT, BM_NAND);
				tgt->InteractCount++;
			}
		}
		ds = dlg->GetState( si );
	} else {
		if (ds->transitionsCount <= choose) {
			return;
		}

		DialogTransition* tr = ds->transitions[choose];

		ta->PopMinRow();

		if (tr->Flags&IE_DLG_TR_JOURNAL) {
			int Section = 0;
			if (tr->Flags&IE_DLG_UNSOLVED) {
				Section |= 1;
			}
			if (tr->Flags&IE_DLG_SOLVED) {
				Section |= 2;
			}
			if (core->GetGame()->AddJournalEntry(tr->journalStrRef, sectionMap[Section], tr->Flags>>16) ) {
				displaymsg->DisplayConstantString(STR_JOURNALCHANGE,0xffff00);
				char *string = core->GetString( tr->journalStrRef );
				//cutting off the strings at the first crlf
				char *poi = strchr(string,'\n');
				if (poi) {
					*poi='\0';
				}
				displaymsg->DisplayString( string );
				free( string );
			}
		}

		if (tr->textStrRef != 0xffffffff) {
			//allow_zero is for PST (deionarra's text)
			displaymsg->DisplayStringName( (int) (tr->textStrRef), 0x8080FF, speaker, IE_STR_SOUND|IE_STR_SPEECH|IE_STR_ALLOW_ZERO);
			if (core->HasFeature( GF_DIALOGUE_SCROLLS )) {
				ta->AppendText( "", -1 );
			}
		}

		if (tr->actions.size()) {
			// does this belong here? we must clear actions somewhere before
			// we start executing them (otherwise queued actions interfere)
			// executing actions directly does not work, because dialog
			// needs to end before final actions are executed due to
			// actions making new dialogs!
			if (target->Type == ST_ACTOR) ((Movable *)target)->ClearPath(); // fuzzie added this
			target->ClearActions();

			for (unsigned int i = 0; i < tr->actions.size(); i++) {
				target->AddAction(tr->actions[i]);
				//GameScript::ExecuteAction( target, action );
			}
		}

		int final_dialog = tr->Flags & IE_DLG_TR_FINAL;

		if (final_dialog) {
			ta->SetMinRow( false );
			EndDialog();
		}

		// *** the commented-out line here should no longer be required, with instant handling ***
		// all dialog actions must be executed immediately
		//target->ProcessActions(true);
		// (do not clear actions - final actions can involve waiting/moving)

		if (final_dialog) {
			return;
		}

		//displaying dialog for selected option
		int si = tr->stateIndex;
		//follow external linkage, if required
		if (tr->Dialog[0] && strnicmp( tr->Dialog, dlg->ResRef, 8 )) {
			//target should be recalculated!
			tgt = NULL;
			if (originalTargetID) {
				// always try original target first (sometimes there are multiple
				// actors with the same dialog in an area, we want to pick the one
				// we were talking to)
				tgt = GetActorByGlobalID(originalTargetID);
				if (tgt && strnicmp( tgt->GetDialog(GD_NORMAL), tr->Dialog, 8 ) != 0) {
					tgt = NULL;
				}
			}
			if (!tgt) {
				// then just search the current area for an actor with the dialog
				tgt = target->GetCurrentArea()->GetActorByDialog(tr->Dialog);
			}
			if (!tgt) {
				// try searching for banter dialogue: the original engine seems to
				// happily let you randomly switch between normal and banter dialogs

				// TODO: work out if this should go somewhere more central (such
				// as GetActorByDialog), or if there's a less awful way to do this
				// (we could cache the entries, for example)
				// TODO: fix for ToB (see also the Interact action)
				AutoTable pdtable("interdia");
				if (pdtable) {
					int row = pdtable->FindTableValue( pdtable->GetColumnIndex("FILE"), tr->Dialog );
					tgt = target->GetCurrentArea()->GetActorByScriptName(pdtable->GetRowName(row));
				}
			}
			target = tgt;
			if (!target) {
				printMessage("Dialog","Can't redirect dialog\n",YELLOW);
				ta->SetMinRow( false );
				EndDialog();
				return;
			}
			Actor *oldTarget = GetActorByGlobalID(targetID);
			targetID = tgt->globalID;
			tgt->SetCircleSize();
			if (oldTarget) oldTarget->SetCircleSize();
			// we have to make a backup, tr->Dialog is freed
			ieResRef tmpresref;
			strnlwrcpy(tmpresref,tr->Dialog, 8);
			if (target->GetInternalFlag()&IF_NOINT) {
				// this whole check moved out of InitDialog by fuzzie, see comments
				// for the IF_NOINT check in BeginDialog
				displaymsg->DisplayConstantString(STR_TARGETBUSY,0xff0000);
				ta->SetMinRow( false );
				EndDialog();
				return;
			}
			int ret = InitDialog( speaker, target, tmpresref);
			if (ret<0) {
				// error was displayed by InitDialog
				ta->SetMinRow( false );
				EndDialog();
				return;
			}
		}
		ds = dlg->GetState( si );
		if (!ds) {
			printMessage("Dialog","Can't find next dialog\n",YELLOW);
			ta->SetMinRow( false );
			EndDialog();
			return;
		}
	}
	//displaying npc text
	displaymsg->DisplayStringName( ds->StrRef, 0x70FF70, target, IE_STR_SOUND|IE_STR_SPEECH);
	//adding a gap between options and npc text
	ta->AppendText("",-1);
	int i;
	int idx = 0;
	ta->SetMinRow( true );
	//first looking for a 'continue' opportunity, the order is descending (a la IE)
	unsigned int x = ds->transitionsCount;
	while(x--) {
		if (ds->transitions[x]->Flags & IE_DLG_TR_FINAL) {
			continue;
		}
		if (ds->transitions[x]->textStrRef != 0xffffffff) {
			continue;
		}
		if (ds->transitions[x]->Flags & IE_DLG_TR_TRIGGER) {
			if (ds->transitions[x]->condition &&
				!ds->transitions[x]->condition->Evaluate(target)) {
				continue;
			}
		}
		core->GetDictionary()->SetAt("DialogOption",x);
		core->GetGameControl()->SetDialogueFlags(DF_OPENCONTINUEWINDOW, BM_OR);
		goto end_of_choose;
	}
	for (x = 0; x < ds->transitionsCount; x++) {
		if (ds->transitions[x]->Flags & IE_DLG_TR_TRIGGER) {
			if (ds->transitions[x]->condition &&
				!ds->transitions[x]->condition->Evaluate(target)) {
				continue;
			}
		}
		idx++;
		if (ds->transitions[x]->textStrRef == 0xffffffff) {
			//dialogchoose should be set to x
			//it isn't important which END option was chosen, as it ends
			core->GetDictionary()->SetAt("DialogOption",x);
			core->GetGameControl()->SetDialogueFlags(DF_OPENENDWINDOW, BM_OR);
		} else {
			char *string = ( char * ) malloc( 40 );
			sprintf( string, "[s=%d,ffffff,ff0000]%d - [p]", x, idx );
			i = ta->AppendText( string, -1 );
			free( string );
			string = core->GetString( ds->transitions[x]->textStrRef );
			ta->AppendText( string, i );
			free( string );
			ta->AppendText( "[/p][/s]", i );
		}
	}
	// this happens if a trigger isn't implemented or the dialog is wrong
	if (!idx) {
		printMessage("Dialog", "There were no valid dialog options!\n", YELLOW);
		core->GetGameControl()->SetDialogueFlags(DF_OPENENDWINDOW, BM_OR);
	}
end_of_choose:
	//padding the rows so our text will be at the top
	if (core->HasFeature( GF_DIALOGUE_SCROLLS )) {
		ta->AppendText( "", -1 );
	}
	else {
		ta->PadMinRow();
	}
}

// TODO: duplicate of the one in GameControl
Actor *DialogHandler::GetActorByGlobalID(ieWord ID)
{
	if (!ID)
		return NULL;
	Game* game = core->GetGame();
	if (!game)
		return NULL;

	Map* area = game->GetCurrentArea( );
	if (!area)
		return NULL;
	return
		area->GetActorByGlobalID(ID);
}

Actor *DialogHandler::GetTarget()
{
	return GetActorByGlobalID(targetID);
}

Actor *DialogHandler::GetSpeaker()
{
	return GetActorByGlobalID(speakerID);
}
