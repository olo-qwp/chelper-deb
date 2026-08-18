/**
 * CHelperTweak —— 我的世界基岩版命令辅助悬浮窗（iOS 越狱插件）
 * 基于 CHelper 开源内核 (GPLv3)
 *
 * 功能：
 *  - 游戏内悬浮球（可拖动）
 *  - 点击展开命令输入面板
 *  - 输入时实时显示补全建议（CHelper 内核）
 *  - 点击建议应用，复制到剪贴板后在游戏聊天框粘贴
 */
#import <UIKit/UIKit.h>
#include <chelper/CHelperCore.h>
#include <pch.h>

@class CHelperTweakAction;

@interface CHelperTweakAction : NSObject
+ (void)ballTap;
+ (void)ballPan:(UIPanGestureRecognizer *)g;
+ (void)textChangedAction:(UITextField *)field;
+ (void)copyAction;
+ (void)closeAction;
@end

static CHelper::CHelperCore *gCore = nullptr;
static UIWindow *gPanelWindow = nil;
static UITextField *gCmdField = nil;
static UITableView *gSugTable = nil;
static NSMutableArray<NSString *> *gSugNames = nil;
static NSMutableArray<NSString *> *gSugDescs = nil;

static NSString *u16ToNSString(const std::u16string &s) {
    std::string utf8;
    utf8::utf16to8(s.begin(), s.end(), std::back_inserter(utf8));
    return [NSString stringWithUTF8String:utf8.c_str()];
}

static std::u16string nsToU16(NSString *s) {
    std::string utf8 = s.UTF8String ?: "";
    std::u16string out;
    utf8::utf8to16(utf8.begin(), utf8.end(), std::back_inserter(out));
    return out;
}

static void initCore() {
    if (gCore) return;
    const char *paths[] = {
        "/usr/share/chelper/release-vanilla-1.21.132.1.cpack",
        "/var/jb/usr/share/chelper/release-vanilla-1.21.132.1.cpack",
        nullptr};
    for (int i = 0; paths[i]; ++i) {
        if ([[NSFileManager defaultManager] fileExistsAtPath:[NSString stringWithUTF8String:paths[i]]]) {
            gCore = CHelper::CHelperCore::createByDirectory(paths[i]);
            if (gCore) return;
        }
    }
}

static void refreshSuggestions() {
    [gSugNames removeAllObjects];
    [gSugDescs removeAllObjects];
    if (!gCore) return;
    auto *sugs = gCore->getSuggestions();
    if (sugs) {
        for (const auto &s : *sugs) {
            [gSugNames addObject:u16ToNSString(s.content->name)];
            if (s.content->description.has_value()) {
                [gSugDescs addObject:u16ToNSString(s.content->description.value())];
            } else {
                [gSugDescs addObject:@""];
            }
        }
    }
    [gSugTable reloadData];
}

@interface CHelperTableDelegate : NSObject <UITableViewDataSource, UITableViewDelegate>
@end
@implementation CHelperTableDelegate
- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section {
    return gSugNames.count;
}
- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath {
    UITableViewCell *cell = [tableView dequeueReusableCellWithIdentifier:@"c"];
    if (!cell) cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleSubtitle reuseIdentifier:@"c"];
    cell.textLabel.text = gSugNames[indexPath.row];
    cell.detailTextLabel.text = gSugDescs[indexPath.row];
    cell.textLabel.textColor = UIColor.whiteColor;
    cell.detailTextLabel.textColor = [UIColor colorWithWhite:0.8 alpha:1];
    cell.backgroundColor = UIColor.clearColor;
    return cell;
}
- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath {
    [tableView deselectRowAtIndexPath:indexPath animated:YES];
    if (!gCore) return;
    auto result = gCore->onSuggestionClick((size_t)indexPath.row);
    if (result.has_value()) {
        gCmdField.text = u16ToNSString(result->first);
        gCore->onTextChanged(result->first, result->second);
        refreshSuggestions();
    }
}
@end

static void onTextChanged(UITextField *field) {
    if (!gCore) return;
    auto u16 = nsToU16(field.text);
    gCore->onTextChanged(u16, u16.size());
    refreshSuggestions();
}

static void copyCommand() {
    UIPasteboard *pb = [UIPasteboard generalPasteboard];
    pb.string = gCmdField.text ?: @"";
    // 轻提示
    UILabel *tip = [[UILabel alloc] initWithFrame:CGRectMake(0, 0, 200, 36)];
    tip.text = @"已复制，去游戏聊天框粘贴吧";
    tip.textColor = UIColor.whiteColor;
    tip.backgroundColor = [UIColor colorWithWhite:0 alpha:0.75];
    tip.textAlignment = NSTextAlignmentCenter;
    tip.layer.cornerRadius = 8;
    tip.clipsToBounds = YES;
    tip.center = CGPointMake(gPanelWindow.bounds.size.width / 2, gPanelWindow.bounds.size.height / 2 - 120);
    [gPanelWindow addSubview:tip];
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(1.2 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
        [tip removeFromSuperview];
    });
}

static void closePanel() {
    gPanelWindow.hidden = YES;
    gPanelWindow = nil;
    gCmdField = nil;
    gSugTable = nil;
}

static void showPanel() {
    if (gPanelWindow) { gPanelWindow.hidden = NO; return; }
    CGSize scr = [UIScreen mainScreen].bounds.size;
    UIWindow *w = [[UIWindow alloc] initWithFrame:CGRectMake(0, 0, scr.width, scr.height)];
    w.windowLevel = 2000.0; // 高于游戏 UI
    w.backgroundColor = [UIColor colorWithWhite:0 alpha:0.55];
    UIViewController *vc = [UIViewController new];
    vc.view.backgroundColor = UIColor.clearColor;
    w.rootViewController = vc;
    gPanelWindow = w;

    CGFloat pw = MIN(scr.width - 24, 420);
    CGFloat ph = 300;
    UIView *panel = [[UIView alloc] initWithFrame:CGRectMake((scr.width - pw) / 2, (scr.height - ph) / 2 - 40, pw, ph)];
    panel.backgroundColor = [UIColor colorWithWhite:0.12 alpha:0.97];
    panel.layer.cornerRadius = 14;
    panel.layer.borderColor = [UIColor colorWithWhite:0.4 alpha:1].CGColor;
    panel.layer.borderWidth = 1;

    UILabel *title = [[UILabel alloc] initWithFrame:CGRectMake(14, 10, pw - 60, 28)];
    title.text = @"命令辅助器 (CHelper)";
    title.textColor = UIColor.whiteColor;
    title.font = [UIFont boldSystemFontOfSize:16];

    UIButton *closeBtn = [UIButton buttonWithType:UIButtonTypeSystem];
    closeBtn.frame = CGRectMake(pw - 44, 8, 36, 32);
    [closeBtn setTitle:@"✕" forState:UIControlStateNormal];
    closeBtn.tintColor = UIColor.whiteColor;
    [closeBtn addTarget:[CHelperTweakAction class] action:@selector(closeAction) forControlEvents:UIControlEventTouchUpInside];

    gCmdField = [[UITextField alloc] initWithFrame:CGRectMake(14, 46, pw - 28, 40)];
    gCmdField.backgroundColor = [UIColor colorWithWhite:0.25 alpha:1];
    gCmdField.textColor = UIColor.whiteColor;
    gCmdField.font = [UIFont monospacedSystemFontOfSize:15 weight:UIFontWeightRegular];
    gCmdField.layer.cornerRadius = 8;
    gCmdField.autocorrectionType = UITextAutocorrectionTypeNo;
    gCmdField.autocapitalizationType = UITextAutocapitalizationTypeNone;
    gCmdField.spellCheckingType = UITextSpellCheckingTypeNo;
    gCmdField.keyboardType = UIKeyboardTypeASCIICapable;
    gCmdField.attributedPlaceholder =
        [[NSAttributedString alloc] initWithString:@"输入命令，如 /give @p diamond"
                                        attributes:@{NSForegroundColorAttributeName: [UIColor colorWithWhite:0.6 alpha:1]}];
    UIView *pad = [[UIView alloc] initWithFrame:CGRectMake(0, 0, 10, 40)];
    gCmdField.leftView = pad;
    gCmdField.leftViewMode = UITextFieldViewModeAlways;
    [gCmdField addTarget:[CHelperTweakAction class] action:@selector(textChangedAction:) forControlEvents:UIControlEventEditingChanged];

    gSugTable = [[UITableView alloc] initWithFrame:CGRectMake(14, 94, pw - 28, ph - 150) style:UITableViewStylePlain];
    gSugTable.backgroundColor = UIColor.clearColor;
    gSugTable.separatorColor = [UIColor colorWithWhite:0.35 alpha:1];
    gSugTable.delegate = [CHelperTableDelegate new];
    gSugTable.dataSource = [CHelperTableDelegate new];

    UIButton *copyBtn = [UIButton buttonWithType:UIButtonTypeSystem];
    copyBtn.frame = CGRectMake(14, ph - 44, pw - 28, 34);
    [copyBtn setTitle:@"复制命令（去聊天框粘贴）" forState:UIControlStateNormal];
    copyBtn.backgroundColor = [UIColor colorWithRed:0.2 green:0.55 blue:0.95 alpha:1];
    copyBtn.tintColor = UIColor.whiteColor;
    copyBtn.layer.cornerRadius = 8;
    [copyBtn addTarget:[CHelperTweakAction class] action:@selector(copyAction) forControlEvents:UIControlEventTouchUpInside];

    [panel addSubview:title];
    [panel addSubview:closeBtn];
    [panel addSubview:gCmdField];
    [panel addSubview:gSugTable];
    [panel addSubview:copyBtn];
    [vc.view addSubview:panel];

    gSugNames = [NSMutableArray new];
    gSugDescs = [NSMutableArray new];

    [w makeKeyAndVisible];
    [gCmdField becomeFirstResponder];
    refreshSuggestions();
}

static void showFloatingBall() {
    CGSize scr = [UIScreen mainScreen].bounds.size;
    UIWindow *ballWin = [[UIWindow alloc] initWithFrame:CGRectMake(scr.width - 74, scr.height * 0.35, 56, 56)];
    ballWin.windowLevel = 1999.0;
    ballWin.backgroundColor = UIColor.clearColor;
    ballWin.rootViewController = [UIViewController new];
    ballWin.userInteractionEnabled = YES;

    UIButton *ball = [UIButton buttonWithType:UIButtonTypeCustom];
    ball.frame = ballWin.bounds;
    ball.layer.cornerRadius = 28;
    ball.backgroundColor = [UIColor colorWithRed:0.2 green:0.7 blue:0.4 alpha:0.92];
    [ball setTitle:@"⛏" forState:UIControlStateNormal];
    ball.titleLabel.font = [UIFont systemFontOfSize:24];
    ball.layer.shadowColor = UIColor.blackColor.CGColor;
    ball.layer.shadowOpacity = 0.4;
    ball.layer.shadowRadius = 6;
    [ball addTarget:[CHelperTweakAction class] action:@selector(ballTap) forControlEvents:UIControlEventTouchUpInside];

    UIPanGestureRecognizer *pan = [[UIPanGestureRecognizer alloc] initWithTarget:[CHelperTweakAction class] action:@selector(ballPan:)];
    [ball addGestureRecognizer:pan];

    [ballWin.rootViewController.view addSubview:ball];
    [ballWin makeKeyAndVisible];
}

@implementation CHelperTweakAction
+ (void)ballTap { showPanel(); }
+ (void)ballPan:(UIPanGestureRecognizer *)g {
    CGPoint p = [g translationInView:g.view.superview];
    g.view.center = CGPointMake(g.view.center.x + p.x, g.view.center.y + p.y);
    [g setTranslation:CGPointZero inView:g.view.superview];
}
+ (void)textChangedAction:(UITextField *)field { onTextChanged(field); }
+ (void)copyAction { copyCommand(); }
+ (void)closeAction { closePanel(); }
@end

__attribute__((constructor))
static void CHelperTweakInit() {
    [[NSNotificationCenter defaultCenter] addObserverForName:UIApplicationDidFinishLaunchingNotification
                                                      object:nil queue:nil usingBlock:^(NSNotification *note) {
        dispatch_async(dispatch_get_main_queue(), ^{
            initCore();
            showFloatingBall();
        });
    }];
}
