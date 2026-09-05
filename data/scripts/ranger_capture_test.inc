RangerCaptureTest_EventScript_Start::
	setstylercapture SPECIES_BEEDRILL, 20
	dostylercapture
	specialvar VAR_RESULT, GetStylerCaptureOutcome
	goto_if_eq VAR_RESULT, STYLER_RESULT_CAUGHT, RangerCaptureTest_EventScript_Caught
	msgbox RangerCaptureTest_Text_BrokeFree, MSGBOX_DEFAULT
	end

RangerCaptureTest_EventScript_Caught:
	msgbox RangerCaptureTest_Text_Caught, MSGBOX_DEFAULT
	end

RangerCaptureTest_Text_Caught:
	.string "You caught it with the styler!$"

RangerCaptureTest_Text_BrokeFree:
	.string "It broke free of the styler.$"
