//
// Created by Phillip Schuster on 04.05.16.
//

#ifndef TEENSY_LOADFILAMENTSCENECONTROLLER_H
#define TEENSY_LOADFILAMENTSCENECONTROLLER_H

#include "SidebarSceneController.h"
#include "BitmapButton.h"
#include "LabelButton.h"

class LoadFilamentSceneController: public SidebarSceneController, public ButtonDelegate
{
#pragma mark Constructor
public:
	LoadFilamentSceneController();
	virtual ~LoadFilamentSceneController();

#pragma mark Sidebar Scene Controller
private:
	virtual const uint8_t *getSidebarIcon() override;
	virtual String getSidebarTitle() const override;

#pragma mark Scene Controller
private:
	String getName();
	virtual void onWillAppear() override;
	virtual uint16_t getBackgroundColor() override;

#pragma mark Button Delegate
private:
	virtual void buttonPressed(void *button) override;

#pragma mark Member Variables
private:
	LabelButton* _button;
};


#endif //TEENSY_LOADFILAMENTSCENECONTROLLER_H
