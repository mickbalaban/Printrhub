/*
 * Sends an error report to MK20 that finally shows some error message on screen
 * if downloads fail
 *
 * Copyright (c) 2016 Printrbot Inc.
 *
 * Developed in cooperation by Phillip Schuster (@appfruits) from appfruits.com
 * http://www.appfruits.com
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "HandleDownloadError.h"
#include "Idle.h"

HandleDownloadError::HandleDownloadError(DownloadError error) :
	Mode(),
	_error(error) {

}

HandleDownloadError::~HandleDownloadError() {

}

void HandleDownloadError::loop() {

}

void HandleDownloadError::onWillStart() {
  EventLogger::log("Starting Download Error mode...");

  uint8_t errorCode = (uint8_t) _error;
  Application.getMK20Stack()->requestTask(TaskID::DownloadError, sizeof(uint8_t), &errorCode);

  Idle *idle = new Idle();
  Application.pushMode(idle);
}

void HandleDownloadError::onWillEnd() {

}

String HandleDownloadError::getName() {
  return "DownloadError";
}

