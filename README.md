Proposed PoW/PoS Hybrid Consensus Mechanism
===========================================

The contributing developers of Project PAI have released a white paper proposing a Hybrid PoW/PoS consensus mechanism. Read the white paper [here](https://projectpai.com/assets/files/whitepaper/Prospective-Hybrid-Consensus-for-Project-PAI.pdf).

PAI Coin Core integration/staging tree
=====================================

https://projectpai.com

What is Project PAI and PAI Coin?
----------------

Project PAI is an open-source, blockchain-based platform designed to allow
everyone to create, manage, and use their own Personal Artificial Intelligence (PAI).
The PAI Blockchain Protocol (PAI blockchain) enables a decentralized AI economy
where application developers can create products and services that will be beneficial
to the PAI ecosystem and users can contribute their PAI data to improve and enhance the
platform's AI algorithms. In addition, companies and developers can easily create
their own token on top of the PAI blockchain to facilitate interaction and transaction
in their own unique experiences. The focal point of all interactions on the PAI
blockchain are PAIs - intelligent 3D avatars that look, talk and behave just like their
human counterparts, made from the digital profiles of the user's online behavior.

PAI Coin is a digital currency that enables instant payments to anyone, anywhere in the world.
PAI Coin uses peer-to-peer technology to operate with no central authority: managing
transactions and issuing money are carried out collectively by the network.
PAI Coin Core is the name of the open-source software which enables the use of this currency.

Read the whitepapers [here](https://projectpai.com/pai-whitepaper/).

License
-------

PAI Coin Core is released under the terms of the MIT license. See [COPYING](COPYING) for more
information or see https://opensource.org/licenses/MIT.

Development Process
-------------------

The `master` branch is regularly built and tested, but is not guaranteed to be
completely stable. [Tags](https://github.com/projectpai/paicoin/tags) are created
regularly to indicate new official, stable release versions of PAI Coin Core.

The contribution workflow is described in [CONTRIBUTING.md](CONTRIBUTING.md).

The developer [forum](https://www.paiforum.com/?forum=665374) should be used 
to discuss complicated or controversial changes before working on a patch set.

Testing
-------

Testing and code review is the bottleneck for development; we get more pull
requests than we can review and test on short notice. Please be patient and help out by testing
other people's pull requests, and remember this is a security-critical project where any mistake might cost people
lots of money.

### Automated Testing

Developers are strongly encouraged to write [unit tests](src/test/README.md) for new code, and to
submit new unit tests for old code. Unit tests can be compiled and run
(assuming they weren't disabled in configure) with: `make check`. Further details on running
and extending unit tests can be found in [/src/test/README.md](/src/test/README.md).

There are also [regression and integration tests](/test), written
in Python, that are run automatically on the build server.
These tests can be run (if the [test dependencies](/test) are installed) with: `test/functional/test_runner.py`

### Manual Quality Assurance (QA) Testing

Changes should be tested by somebody other than the developer who wrote the
code. This is especially important for large or high-risk changes. It is useful
to add a test plan to the pull request description if testing the changes is
not straightforward.

Community Resources
-------------------

### English

- [PAI Forum](https://www.paiforum.com)
- [Telegram (USA)](https://t.me/projectpai)
- [Telegram (EU)](https://t.me/projectpaiEUR)
- [Twitter](https://twitter.com/projectpai)
- [Facebook](https://www.facebook.com/projectpai)
- [LinkedIn](https://www.linkedin.com/company/projectpai/)
- [Medium](https://medium.com/project-pai)
- [YouTube](https://www.youtube.com/c/projectpai)

### Chinese

- [Telegram](https://t.me/projectpaiCN)
- [Weibo](https://weibo.com/p/1005055231677544/)
- [Youku](http://i.youku.com/i/UNjA4ODk0MDE3Ng==)
- WeChat

### Japanese

- [Telegram](https://t.me/ProjectPAIJapanOFFICIAL)
- [Line](http://line.me/ti/g/ZeZog_6Gbc)
- [Ameba](https://ameblo.jp/projectpai/)

### Korean

- [Telegram](https://t.me/ProjectPAIKoreaOFFICIAL)
- [Kakao](https://open.kakao.com/o/ggXD4VN)
- [Naver](https://blog.naver.com/projectpai/)
