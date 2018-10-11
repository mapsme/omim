#import "platform/http_session_manager.h"

@interface DataTaskInfo: NSObject

@property (nonatomic, strong) id<NSURLSessionDataDelegate> delegate;
@property (nonatomic, strong) NSURLSessionDataTask * task;

- (instancetype) initWithTask:(NSURLSessionDataTask *)task delegate:(id<NSURLSessionDataDelegate>)delegate;

@end

@implementation DataTaskInfo

- (instancetype) initWithTask:(NSURLSessionDataTask *)task delegate:(id<NSURLSessionDataDelegate>)delegate
{
    self = [super init];
    if (self != nil) {
        _task = task;
        _delegate = delegate;
    }
    return self;
}

@end

@interface HttpSessionManager()<NSURLSessionDataDelegate>

@property (nonatomic, strong) NSURLSession * session;
@property (nonatomic, strong) NSMutableDictionary * taskInfoByTaskID;

@end

@implementation HttpSessionManager

- (instancetype) init
{
    return [self initWithConfiguration:nil];
}

- (instancetype) initWithConfiguration:(NSURLSessionConfiguration *)configuration
{
    self = [super init];
    if (self != nil) {
        if (configuration == nil) {
            configuration = [NSURLSessionConfiguration defaultSessionConfiguration];
        }
        
        _session = [NSURLSession sessionWithConfiguration:configuration delegate:self delegateQueue:[NSOperationQueue mainQueue]];
        _taskInfoByTaskID = [NSMutableDictionary dictionary];
    }
    return self;
}

- (NSURLSessionDataTask *) dataTaskWithRequest:(NSURLRequest *)request delegate:(id<NSURLSessionDataDelegate>)delegate completionHandler:(void (^)(NSData * data, NSURLResponse * response, NSError * error))completionHandler
{
    NSURLSessionDataTask * task = [self.session dataTaskWithRequest:request completionHandler:completionHandler];
    DataTaskInfo * taskInfo = [[DataTaskInfo alloc] initWithTask:task delegate:delegate];
    
    dispatch_async(dispatch_get_main_queue(), ^{
        self.taskInfoByTaskID[task] = taskInfo;
    });
    
    return task;
}

- (DataTaskInfo *) taskInfoForTask:(NSURLSessionTask *)task
{
    return self.taskInfoByTaskID[task];
}

- (void) URLSession:(NSURLSession *)session task:(NSURLSessionTask *)task willPerformHTTPRedirection:(NSHTTPURLResponse *)response newRequest:(NSURLRequest *)newRequest completionHandler:(void (^)(NSURLRequest *))completionHandler
{
    DataTaskInfo * taskInfo = [self taskInfoForTask:task];
    if ([taskInfo.delegate respondsToSelector:@selector(URLSession:task:willPerformHTTPRedirection:newRequest:completionHandler:)]) {
        [taskInfo.delegate URLSession:session task:task willPerformHTTPRedirection:response newRequest:newRequest completionHandler:completionHandler];
    } else {
        completionHandler(newRequest);
    }
}

- (void) URLSession:(NSURLSession *)session task:(NSURLSessionTask *)task didCompleteWithError:(NSError *)error
{
    DataTaskInfo * taskInfo = [self taskInfoForTask:task];
    [self.taskInfoByTaskID removeObjectForKey:@(taskInfo.task.taskIdentifier)];

    if ([taskInfo.delegate respondsToSelector:@selector(URLSession:task:didCompleteWithError:)]) {
        [taskInfo.delegate URLSession:session task:task didCompleteWithError:error];
    }
}

- (void) URLSession:(NSURLSession *)session dataTask:(NSURLSessionDataTask *)dataTask didReceiveResponse:(NSURLResponse *)response completionHandler:(void (^)(NSURLSessionResponseDisposition disposition))completionHandler
{
    DataTaskInfo * taskInfo = [self taskInfoForTask:dataTask];
    if ([taskInfo.delegate respondsToSelector:@selector(URLSession:dataTask:didReceiveResponse:completionHandler:)]) {
        [taskInfo.delegate URLSession:session dataTask:dataTask didReceiveResponse:response completionHandler:completionHandler];
    } else {
        completionHandler(NSURLSessionResponseAllow);
    }
}

- (void) URLSession:(NSURLSession *)session dataTask:(NSURLSessionDataTask *)dataTask didReceiveData:(NSData *)data
{
    DataTaskInfo * taskInfo = [self taskInfoForTask:dataTask];
    if ([taskInfo.delegate respondsToSelector:@selector(URLSession:dataTask:didReceiveData:)]) {
        [taskInfo.delegate URLSession:session dataTask:dataTask didReceiveData:data];
    }
}

#if DEBUG
- (void) URLSession:(NSURLSession *)session didReceiveChallenge:(NSURLAuthenticationChallenge *)challenge completionHandler:(void (^)(NSURLSessionAuthChallengeDisposition disposition, NSURLCredential * _Nullable credential))completionHandler
{
    NSURLCredential * credential = [[NSURLCredential alloc] initWithTrust:[challenge protectionSpace].serverTrust];
    completionHandler(NSURLSessionAuthChallengeUseCredential, credential);
}
#endif

@end
