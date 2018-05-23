//
//  ActionSheetMultipleStringPicker.h
//  CoreActionSheetPicker
//
//  Created by Alejandro on 21/07/15.
//  Copyright (c) 2015 Petr Korolev. All rights reserved.
//
//Redistribution and use in source and binary forms, with or without
//modification, are permitted provided that the following conditions are met:
//* Redistributions of source code must retain the above copyright
//notice, this list of conditions and the following disclaimer.
//* Redistributions in binary form must reproduce the above copyright
//notice, this list of conditions and the following disclaimer in the
//documentation and/or other materials provided with the distribution.
//* Neither the name of the <organization> nor the
//names of its contributors may be used to endorse or promote products
//derived from this software without specific prior written permission.
//
//THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
//ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
//WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
//DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
//DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
//(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
//åLOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
//ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
//(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
//SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//

#import "AbstractActionSheetPicker.h"

@class ActionSheetMultipleStringPicker;
typedef void(^ActionMultipleStringDoneBlock)(ActionSheetMultipleStringPicker *picker, NSArray *selectedIndexes, id selectedValues);
typedef void(^ActionMultipleStringCancelBlock)(ActionSheetMultipleStringPicker *picker);

@interface ActionSheetMultipleStringPicker : AbstractActionSheetPicker <UIPickerViewDelegate, UIPickerViewDataSource>
/**
 *  Create and display an action sheet picker.
 *
 *  @param title             Title label for picker
 *  @param data              is an array of strings to use for the picker's available selection choices
 *  @param indexes           is used to establish the initially selected row;
 *  @param target            must not be empty.  It should respond to "onSuccess" actions.
 *  @param successAction     successAction
 *  @param cancelActionOrNil cancelAction
 *  @param origin            must not be empty.  It can be either an originating container view or a UIBarButtonItem to use with a popover arrow.
 *
 *  @return  return instance of picker
 */
+ (instancetype)showPickerWithTitle:(NSString *)title rows:(NSArray *)data initialSelection:(NSArray *)indexes target:(id)target successAction:(SEL)successAction cancelAction:(SEL)cancelActionOrNil origin:(id)origin;

// Create an action sheet picker, but don't display until a subsequent call to "showActionPicker".  Receiver must release the picker when ready. */
- (instancetype)initWithTitle:(NSString *)title rows:(NSArray *)data initialSelection:(NSArray *)indexes target:(id)target successAction:(SEL)successAction cancelAction:(SEL)cancelActionOrNil origin:(id)origin;



+ (instancetype)showPickerWithTitle:(NSString *)title rows:(NSArray *)strings initialSelection:(NSArray *)indexes doneBlock:(ActionMultipleStringDoneBlock)doneBlock cancelBlock:(ActionMultipleStringCancelBlock)cancelBlock origin:(id)origin;

- (instancetype)initWithTitle:(NSString *)title rows:(NSArray *)strings initialSelection:(NSArray *)indexes doneBlock:(ActionMultipleStringDoneBlock)doneBlock cancelBlock:(ActionMultipleStringCancelBlock)cancelBlockOrNil origin:(id)origin;

@property (nonatomic, copy) ActionMultipleStringDoneBlock onActionSheetDone;
@property (nonatomic, copy) ActionMultipleStringCancelBlock onActionSheetCancel;

@end
