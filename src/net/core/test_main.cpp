// #include "net/LogicSystem.hpp"
// #include "net/MsgNode.hpp"
// #include <cstdio>
// #include <iostream>
// #include <memory>

// void testFunc(std::shared_ptr<msgNode> node) {
//     std::cout << "msg: " << node->Getdata() << std::endl;
//     std::cout << "len: " << node->GetLen() << std::endl;
// }

// int main() {
//     char *data = "Hello, world!";
//     std::shared_ptr<Send_Node> sNode =
//         std::make_shared<Send_Node>(data, strlen(data));
//     testFunc(sNode);
//     sNode->id_ = MSG_IDS::RTP_SEND_PKT;
//     std::shared_ptr<LogicNode> lnode =
//         std::make_shared<LogicNode>(nullptr, sNode);

//     std::cout<<lnode->node_->Getdata();
// }
